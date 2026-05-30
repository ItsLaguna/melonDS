// src/frontend/qt_sdl/DiscordIPC.cpp

#include "DiscordIPC.h"

#include <QStandardPaths>
#include <QByteArray>
#include <QString>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>

// ---------------------------------------------------------------------------
// Socket path — Discord tries /run/user/<uid>/discord-ipc-0 through -9,
// with fallbacks to $TMPDIR and /tmp.
// ---------------------------------------------------------------------------
QByteArray DiscordIPC::socketPath() const
{
    const QByteArray runtimeDir = qgetenv("XDG_RUNTIME_DIR");
    const QByteArray tmpDir     = qgetenv("TMPDIR");

    QList<QByteArray> bases;
    if (!runtimeDir.isEmpty()) bases << runtimeDir;
    if (!tmpDir.isEmpty())     bases << tmpDir;
    bases << QByteArray("/tmp");
    // snap/flatpak sandbox paths
    if (!runtimeDir.isEmpty())
    {
        bases << (runtimeDir + "/snap.discord");
        bases << (runtimeDir + "/app/com.discordapp.Discord");
    }

    for (const QByteArray& base : bases)
        for (int i = 0; i < 10; ++i)
        {
            QByteArray path = base + "/discord-ipc-" + QByteArray::number(i);
            if (::access(path.constData(), F_OK) == 0)
                return path;
        }
    return {};
}

// ---------------------------------------------------------------------------
// connect / close
// ---------------------------------------------------------------------------
bool DiscordIPC::connect(const char* clientID)
{
    if (m_fd >= 0) close();

    QByteArray path = socketPath();
    if (path.isEmpty()) return false;

    m_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_fd < 0) return false;

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    ::strncpy(addr.sun_path, path.constData(), sizeof(addr.sun_path) - 1);

    if (::connect(m_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0)
    {
        ::close(m_fd);
        m_fd = -1;
        return false;
    }

    // Send handshake: {"v":1,"client_id":"<id>"}
    QByteArray handshake = R"({"v":1,"client_id":")" + QByteArray(clientID) + R"("})";
    if (!writeFrame(OP_HANDSHAKE, handshake))
    {
        ::close(m_fd);
        m_fd = -1;
        return false;
    }

    // Read handshake response (READY event) — we don't validate it, just drain it
    Opcode op;
    QByteArray resp;
    readFrame(op, resp);

    return true;
}

void DiscordIPC::close()
{
    if (m_fd >= 0)
    {
        // Send OP_CLOSE politely
        writeFrame(OP_CLOSE, R"({"v":1,"client_id":"0"})");
        ::close(m_fd);
        m_fd = -1;
    }
}

// ---------------------------------------------------------------------------
// Activity
// ---------------------------------------------------------------------------
bool DiscordIPC::setActivity(const char* details,
                              const char* state,
                              const char* largeImageKey,
                              const char* largeImageText,
                              const char* smallImageKey,
                              const char* smallImageText,
                              int64_t     startTimestamp)
{
    if (m_fd < 0) return false;

    QByteArray payload = buildSetActivityPayload(
        details, state,
        largeImageKey, largeImageText,
        smallImageKey, smallImageText,
        startTimestamp, m_nonce++);

    return writeFrame(OP_FRAME, payload);
}

bool DiscordIPC::clearActivity()
{
    if (m_fd < 0) return false;
    return writeFrame(OP_FRAME, buildClearPayload(m_nonce++));
}

// ---------------------------------------------------------------------------
// Low-level read/write
// ---------------------------------------------------------------------------
bool DiscordIPC::writeFrame(Opcode opcode, const QByteArray& payload)
{
    Header hdr;
    hdr.opcode = static_cast<uint32_t>(opcode);
    hdr.length = static_cast<uint32_t>(payload.size());

    if (::write(m_fd, &hdr, sizeof(hdr)) != sizeof(hdr))     return false;
    if (::write(m_fd, payload.constData(), payload.size())
            != static_cast<ssize_t>(payload.size()))          return false;
    return true;
}

bool DiscordIPC::readFrame(Opcode& outOpcode, QByteArray& outPayload)
{
    Header hdr;
    ssize_t n = ::read(m_fd, &hdr, sizeof(hdr));
    if (n != sizeof(hdr)) return false;

    outOpcode = static_cast<Opcode>(hdr.opcode);
    outPayload.resize(hdr.length);

    ssize_t total = 0;
    while (total < static_cast<ssize_t>(hdr.length))
    {
        n = ::read(m_fd, outPayload.data() + total, hdr.length - total);
        if (n <= 0) return false;
        total += n;
    }
    return true;
}

// ---------------------------------------------------------------------------
// JSON helpers
// ---------------------------------------------------------------------------
QByteArray DiscordIPC::jsonEscape(const char* str)
{
    if (!str) return {};
    QByteArray out;
    for (const char* p = str; *p; ++p)
    {
        switch (*p)
        {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += *p;     break;
        }
    }
    return out;
}

QByteArray DiscordIPC::buildSetActivityPayload(
    const char* details,
    const char* state,
    const char* largeImageKey,
    const char* largeImageText,
    const char* smallImageKey,
    const char* smallImageText,
    int64_t     startTimestamp,
    int         nonce)
{
    // Build "assets" object
    QByteArray assets = "{";
    if (largeImageKey && *largeImageKey)
    {
        assets += R"("large_image":")" + jsonEscape(largeImageKey) + '"';
        if (largeImageText && *largeImageText)
            assets += R"(,"large_text":")" + jsonEscape(largeImageText) + '"';
    }
    if (smallImageKey && *smallImageKey)
    {
        if (assets.size() > 1) assets += ',';
        assets += R"("small_image":")" + jsonEscape(smallImageKey) + '"';
        if (smallImageText && *smallImageText)
            assets += R"(,"small_text":")" + jsonEscape(smallImageText) + '"';
    }
    assets += '}';

    // Build "activity" object
    QByteArray activity = "{";
    if (details && *details)
        activity += R"("details":")" + jsonEscape(details) + '"';
    if (state && *state)
    {
        if (activity.size() > 1) activity += ',';
        activity += R"("state":")" + jsonEscape(state) + '"';
    }
    if (startTimestamp > 0)
    {
        if (activity.size() > 1) activity += ',';
        activity += R"("timestamps":{"start":)" + QByteArray::number(startTimestamp) + '}';
    }
    if (assets.size() > 2) // not just "{}"
    {
        if (activity.size() > 1) activity += ',';
        activity += R"("assets":)" + assets;
    }
    activity += '}';

    // Wrap in the SET_ACTIVITY command envelope
    QByteArray payload =
        R"({"cmd":"SET_ACTIVITY","args":{"pid":)" +
        QByteArray::number(static_cast<int>(::getpid())) +
        R"(,"activity":)" + activity +
        R"(},"nonce":")" + QByteArray::number(nonce) + R"("})";

    return payload;
}

QByteArray DiscordIPC::buildClearPayload(int nonce)
{
    return QByteArray(
        R"({"cmd":"SET_ACTIVITY","args":{"pid":)" +
        QByteArray::number(static_cast<int>(::getpid())) +
        R"(,"activity":null},"nonce":")" +
        QByteArray::number(nonce) + R"("})");
}
