// src/frontend/qt_sdl/DiscordPresence.cpp

#include "DiscordPresence.h"
#include "LibraryEntry.h"
#include "Config.h"

#include <QDateTime>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QMetaObject>
#include <QtConcurrent/QtConcurrent>

#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>

DiscordPresence& DiscordPresence::get()
{
    static DiscordPresence instance;
    return instance;
}

DiscordPresence::DiscordPresence(QObject* parent) : QObject(parent) {}
DiscordPresence::~DiscordPresence() { shutdown(); }

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
void DiscordPresence::loadConfig()
{
    Config::Table cfg = Config::GetGlobalTable();
    m_enabled = cfg.GetBool("DiscordPresence.Enabled");
}

void DiscordPresence::init(bool skipInitialPresence)
{
    loadConfig();
    if (!m_enabled || m_initialized) return;

    Config::Table cfg = Config::GetGlobalTable();
    std::string appID = cfg.GetString("DiscordPresence.AppID");
    if (appID.empty()) appID = DEFAULT_CLIENT_ID;

    if (!m_ipc.connect(appID.c_str())) return;
    m_initialized = true;

    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setInterval(15000);
    connect(m_reconnectTimer, &QTimer::timeout, this, &DiscordPresence::reconnectIfNeeded);
    m_reconnectTimer->start();

    if (!skipInitialPresence)
        clearROM();
}

void DiscordPresence::reinit()
{
    // This may be called from a background thread (via QtConcurrent from the dialog).
    // Do the blocking parts here (socket close/reconnect), then marshal all
    // Qt object creation (QTimer, QFutureWatcher) back to the main thread.

    const bool hadGame       = !m_cachedGameCode.isEmpty();
    QByteArray savedDetails  = m_details;
    QByteArray savedLargeText= m_largeText;
    int64_t    savedTs       = m_startTimestamp;
    QByteArray savedGameCode = m_cachedGameCode;

    // Bump generation and cancel watchers — must be done on the main thread
    // since m_activeWatchers and m_generation are UI-thread state.
    QMetaObject::invokeMethod(this, [this]()
    {
        m_generation++;
        cancelPendingWatchers();
        if (m_reconnectTimer) { m_reconnectTimer->stop(); m_reconnectTimer = nullptr; }
        m_ipc.close();
        m_initialized = false;
    }, Qt::BlockingQueuedConnection);

    // Now do the blocking socket connect off the UI thread (we're already on a worker)
    bool connected = false;
    {
        Config::Table cfg = Config::GetGlobalTable();
        std::string appID = cfg.GetString("DiscordPresence.AppID");
        if (appID.empty()) appID = DEFAULT_CLIENT_ID;

        Config::Table cfgEnabled = Config::GetGlobalTable();
        if (!cfgEnabled.GetBool("DiscordPresence.Enabled")) return;

        connected = m_ipc.connect(appID.c_str());
    }

    // Marshal the rest back to the main thread
    QMetaObject::invokeMethod(this, [this, connected, hadGame,
                                     savedDetails, savedLargeText,
                                     savedTs, savedGameCode]()
    {
        if (!connected) return;

        m_initialized = true;

        // Restart the reconnect timer
        m_reconnectTimer = new QTimer(this);
        m_reconnectTimer->setInterval(15000);
        connect(m_reconnectTimer, &QTimer::timeout, this, &DiscordPresence::reconnectIfNeeded);
        m_reconnectTimer->start();

        if (hadGame)
        {
            Config::Table cfg = Config::GetGlobalTable();
            const bool showCode    = cfg.GetBool("DiscordPresence.ShowGameCode");
            const QString gameCode = QString::fromUtf8(savedGameCode);

            m_details        = savedDetails;
            m_largeText      = savedLargeText;
            m_startTimestamp = savedTs;
            m_cachedGameCode = savedGameCode;
            m_state          = (showCode ? gameCode.toUpper() : regionName(gameCode)).toUtf8();

            const QString url = coverUrl(gameCode);
            if (url == QLatin1String("melonds_logo"))
            {
                m_largeKey = "melonds_logo";
                applyCurrentPresence();
            }
            else
            {
                m_largeKey = url.toUtf8();
                const QString fallbackUrl = coverUrlForRegion(gameCode, QStringLiteral("EN"));
                startUrlCheck(url, fallbackUrl);
            }
        }
        else
        {
            clearROM();
        }
    }, Qt::QueuedConnection);
}

void DiscordPresence::cancelPendingWatchers()
{
    for (auto* w : m_activeWatchers)
    {
        w->disconnect();  // prevent finished() from firing into our (possibly destroyed) state
        // Do NOT call w->cancel() — it blocks until the thread finishes.
        // The generation check in the lambda handles stale results safely.
        w->deleteLater();
    }
    m_activeWatchers.clear();
}

void DiscordPresence::shutdown()
{
    m_generation++;        // invalidates all in-flight watcher callbacks
    cancelPendingWatchers();
    if (m_reconnectTimer) { m_reconnectTimer->stop(); m_reconnectTimer = nullptr; }
    m_ipc.close();
    m_initialized = false;
}

void DiscordPresence::reconnectIfNeeded()
{
    if (m_ipc.isConnected()) return;

    Config::Table cfg = Config::GetGlobalTable();
    std::string appID = cfg.GetString("DiscordPresence.AppID");
    if (appID.empty()) appID = DEFAULT_CLIENT_ID;

    if (m_ipc.connect(appID.c_str()))
    {
        m_initialized = true;
        if (!m_cachedGameCode.isEmpty())
            applyCurrentPresence();
        else
            clearROM();
    }
}

// ---------------------------------------------------------------------------
// Presence updates
// ---------------------------------------------------------------------------
void DiscordPresence::startUrlCheck(const QString& primaryUrl, const QString& fallbackUrl)
{
    const int gen = m_generation; // capture current generation

    auto* watcher = new QFutureWatcher<bool>(this);
    m_activeWatchers.append(watcher);

    connect(watcher, &QFutureWatcher<bool>::finished, this,
            [this, watcher, primaryUrl, fallbackUrl, gen]()
    {
        m_activeWatchers.removeOne(watcher);
        watcher->deleteLater();

        // Stale callback — reinit or shutdown happened since this check was launched
        if (gen != m_generation) return;

        if (watcher->result())
        {
            m_largeKey = primaryUrl.toUtf8();
            applyCurrentPresence();
            return;
        }

        // Primary 404'd — try EN fallback if it differs
        if (primaryUrl == fallbackUrl)
        {
            m_largeKey = "melonds_logo";
            applyCurrentPresence();
            return;
        }

        auto* watcher2 = new QFutureWatcher<bool>(this);
        m_activeWatchers.append(watcher2);
        connect(watcher2, &QFutureWatcher<bool>::finished, this,
                [this, watcher2, fallbackUrl, gen]()
        {
            m_activeWatchers.removeOne(watcher2);
            watcher2->deleteLater();

            if (gen != m_generation) return;

            m_largeKey = watcher2->result()
                ? fallbackUrl.toUtf8()
                : QByteArray("melonds_logo");
            applyCurrentPresence();
        });
        watcher2->setFuture(QtConcurrent::run(&DiscordPresence::urlExists, fallbackUrl));
    });

    watcher->setFuture(QtConcurrent::run(&DiscordPresence::urlExists, primaryUrl));
}

void DiscordPresence::applyCurrentPresence()
{
    if (!m_ipc.isConnected()) return;
    m_ipc.setActivity(
        m_details.constData(),
        m_state.constData(),
        m_largeKey.constData(),
        m_largeText.constData(),
        "melonds_logo", "melonDS",
        m_startTimestamp);
}

void DiscordPresence::setROMLoaded(const melonds::LibraryEntry& entry)
{
    if (!m_enabled) return;
    if (!m_ipc.isConnected()) reconnectIfNeeded();
    if (!m_ipc.isConnected()) return;

    QString displayName;
    if (!entry.title.isEmpty())
        displayName = entry.title;
    else if (!entry.filename.isEmpty())
        displayName = QFileInfo(entry.filename).completeBaseName();
    else
        displayName = QStringLiteral("Unknown Game");

    Config::Table cfg = Config::GetGlobalTable();
    const bool showCode = cfg.GetBool("DiscordPresence.ShowGameCode");
    QString stateLine = entry.gameCode.isEmpty()
        ? QStringLiteral("NDS")
        : (showCode ? entry.gameCode.toUpper() : regionName(entry.gameCode));

    m_startTimestamp = QDateTime::currentSecsSinceEpoch();
    m_details        = displayName.toUtf8();
    m_state          = stateLine.toUtf8();
    m_largeText      = displayName.toUtf8();
    m_cachedGameCode = entry.gameCode.toUtf8();
    m_initialized    = true;

    const QString url = coverUrl(entry.gameCode);

    if (url == QLatin1String("melonds_logo"))
    {
        // "none" art type or empty game code — use logo, no network check
        m_largeKey = "melonds_logo";
        applyCurrentPresence();
        return;
    }

    // Validate URL on a background thread.
    // Fallback chain: primary region URL → EN region URL → melonds_logo
    // Generation is captured so stale callbacks from a previous session are ignored.
    m_largeKey = url.toUtf8();
    const QString fallbackUrl = coverUrlForRegion(entry.gameCode, QStringLiteral("EN"));
    startUrlCheck(url, fallbackUrl);
}

void DiscordPresence::clearROM()
{
    m_startTimestamp = 0;
    m_details.clear();
    m_state.clear();
    m_cachedGameCode.clear();
    m_largeKey  = "melonds_logo";
    m_largeText = "melonDS";

    if (m_ipc.isConnected())
        m_ipc.setActivity("Picking game...", "In library", "melonds_logo", "melonDS",
                          nullptr, nullptr, 0);
}

// ---------------------------------------------------------------------------
// URL existence check — runs on background thread via QtConcurrent.
// Uses SO_RCVTIMEO / SO_SNDTIMEO so the socket never blocks longer than
// kTimeoutSec seconds, keeping reinit/shutdown responsive.
// ---------------------------------------------------------------------------
bool DiscordPresence::urlExists(const QString& url)
{
    static constexpr int kTimeoutSec = 4;

    QString stripped = url;
    if (stripped.startsWith("https://"))     stripped = stripped.mid(8);
    else if (stripped.startsWith("http://")) stripped = stripped.mid(7);

    const int slash = stripped.indexOf('/');
    if (slash < 0) return false;
    const QByteArray host = stripped.left(slash).toUtf8();
    const QByteArray path = stripped.mid(slash).toUtf8();

    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (::getaddrinfo(host.constData(), "80", &hints, &res) != 0)
        return false;

    int fd = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) { ::freeaddrinfo(res); return false; }

    // Set send/receive timeouts so we never hang longer than kTimeoutSec
    struct timeval tv { kTimeoutSec, 0 };
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (::connect(fd, res->ai_addr, res->ai_addrlen) != 0)
    {
        ::freeaddrinfo(res);
        ::close(fd);
        return false;
    }
    ::freeaddrinfo(res);

    const QByteArray req =
        "HEAD " + path + " HTTP/1.0\r\n"
        "Host: " + host + "\r\n"
        "Connection: close\r\n\r\n";

    if (::write(fd, req.constData(), req.size()) != req.size())
    {
        ::close(fd);
        return false;
    }

    char buf[64];
    const int n = ::read(fd, buf, sizeof(buf) - 1);
    ::close(fd);
    if (n <= 0) return false;
    buf[n] = '\0';

    const char* sp = ::strchr(buf, ' ');
    if (!sp) return false;
    const int status = ::atoi(sp + 1);
    return (status >= 200 && status < 300);
}

// ---------------------------------------------------------------------------
// Region / URL helpers
// ---------------------------------------------------------------------------
QString DiscordPresence::gametdbRegion(const QString& gameCode)
{
    if (gameCode.length() < 4) return QStringLiteral("US");
    switch (gameCode[3].toLatin1())
    {
        case 'E': return QStringLiteral("US");
        case 'J': return QStringLiteral("JA");
        case 'P': return QStringLiteral("EN"); // Europe — GameTDB uses EN, not EU
        case 'D': return QStringLiteral("DE");
        case 'F': return QStringLiteral("FR");
        case 'S': return QStringLiteral("ES");
        case 'I': return QStringLiteral("IT");
        case 'H': return QStringLiteral("NL");
        case 'K': return QStringLiteral("KO");
        case 'C': return QStringLiteral("ZH");
        case 'T': return QStringLiteral("ZH");
        case 'A': return QStringLiteral("AU");
        default:  return QStringLiteral("EN");
    }
}

QString DiscordPresence::regionName(const QString& gameCode)
{
    if (gameCode.length() < 4) return QStringLiteral("Unknown Region");
    switch (gameCode[3].toLatin1())
    {
        case 'J': return QStringLiteral("Japan");
        case 'E': return QStringLiteral("USA");
        case 'P': return QStringLiteral("Europe");
        case 'D': return QStringLiteral("Germany");
        case 'F': return QStringLiteral("France");
        case 'S': return QStringLiteral("Spain");
        case 'I': return QStringLiteral("Italy");
        case 'H': return QStringLiteral("Netherlands");
        case 'K': return QStringLiteral("Korea");
        case 'C': return QStringLiteral("China");
        case 'T': return QStringLiteral("Taiwan");
        case 'A': return QStringLiteral("Australia");
        default:  return QStringLiteral("Unknown Region");
    }
}

QString DiscordPresence::coverUrl(const QString& gameCode)
{
    if (gameCode.isEmpty()) return QStringLiteral("melonds_logo");

    Config::Table cfg = Config::GetGlobalTable();
    const QString artType = cfg.GetQString("DiscordPresence.ArtType");

    if (artType.startsWith(QLatin1String("none")))
        return QStringLiteral("melonds_logo");

    return coverUrlForRegion(gameCode, gametdbRegion(gameCode));
}

QString DiscordPresence::coverUrlForRegion(const QString& gameCode, const QString& region)
{
    if (gameCode.isEmpty()) return QStringLiteral("melonds_logo");

    Config::Table cfg = Config::GetGlobalTable();
    const QString artType = cfg.GetQString("DiscordPresence.ArtType");

    if (artType.startsWith(QLatin1String("none")))
        return QStringLiteral("melonds_logo");

    QString type, ext;
    if (!artType.isEmpty() && artType.contains('|'))
    {
        type = artType.section('|', 0, 0);
        ext  = artType.section('|', 1, 1);
    }
    else
    {
        type = QStringLiteral("cover");
        ext  = QStringLiteral("jpg");
    }

    return QStringLiteral("https://art.gametdb.com/ds/%1/%2/%3.%4")
               .arg(type, region, gameCode.toUpper(), ext);
}
