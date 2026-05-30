#pragma once
// src/frontend/qt_sdl/DiscordPresence.h

#include "DiscordIPC.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QTimer>
#include <cstdint>

template<typename T> class QFutureWatcher;

namespace melonds { struct LibraryEntry; }

class DiscordPresence : public QObject
{
    Q_OBJECT

public:
    // Default Client ID — replace with your own from https://discord.com/developers/applications
    static constexpr const char* DEFAULT_CLIENT_ID = "1509302127560884396";

    static DiscordPresence& get();

    void init(bool skipInitialPresence = false);
    void reinit();
    void shutdown();

    void setROMLoaded(const melonds::LibraryEntry& entry);
    void clearROM();

    bool isInitialized() const { return m_initialized; }
    bool isEnabled()     const { return m_enabled; }

private:
    explicit DiscordPresence(QObject* parent = nullptr);
    ~DiscordPresence() override;

    void loadConfig();
    void reconnectIfNeeded();

    // Sends current m_details/m_state/m_largeKey/m_largeText to Discord.
    void applyCurrentPresence();
    void startUrlCheck(const QString& primaryUrl, const QString& fallbackUrl);
    void cancelPendingWatchers();

    static QString gametdbRegion(const QString& gameCode);
    static QString regionName(const QString& gameCode);

    // Returns "melonds_logo" when art type is "none" or gameCode is empty,
    // otherwise builds the GameTDB URL for the game's native region.
    static QString coverUrl(const QString& gameCode);

    // Builds a GameTDB URL for a specific region folder (used for EN fallback).
    static QString coverUrlForRegion(const QString& gameCode, const QString& region);

    // Synchronous HEAD request — runs on a background thread via QtConcurrent.
    // Returns true if the URL responds with a 2xx status.
    static bool urlExists(const QString& url);

    DiscordIPC m_ipc;
    QTimer*    m_reconnectTimer = nullptr;
    bool       m_initialized    = false;
    bool       m_enabled        = true;
    int64_t    m_startTimestamp = 0;
    int        m_generation     = 0; // bumped on shutdown/reinit to invalidate stale watcher callbacks

    QByteArray m_details;
    QByteArray m_state;
    QByteArray m_largeKey;
    QByteArray m_largeText;
    QByteArray m_cachedGameCode; // stored so reinit can re-derive URL/state from new config

    QList<QFutureWatcher<bool>*> m_activeWatchers;
};
