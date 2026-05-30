#pragma once
// src/frontend/qt_sdl/DiscordIPC.h
//
// Minimal Discord IPC client — implements only what's needed for Rich Presence.
// No external dependencies. Supports Linux/macOS (Unix socket) only;
// Windows would need named pipes but melonDS is primarily Linux/mac anyway.
//
// Protocol reference: https://discord.com/developers/docs/topics/rpc

#include <QString>
#include <QByteArray>
#include <cstdint>

class DiscordIPC
{
public:
    DiscordIPC() = default;
    ~DiscordIPC() { close(); }

    // Connect to Discord's IPC socket and perform the handshake.
    // Returns true on success.
    bool connect(const char* clientID);

    // Send a SET_ACTIVITY command.
    // Pass nullptr/empty string to clear presence.
    bool setActivity(const char* details,
                     const char* state,
                     const char* largeImageKey,
                     const char* largeImageText,
                     const char* smallImageKey,
                     const char* smallImageText,
                     int64_t     startTimestamp);

    bool clearActivity();

    void close();

    bool isConnected() const { return m_fd >= 0; }

private:
    // Discord IPC opcodes
    enum Opcode : uint32_t
    {
        OP_HANDSHAKE = 0,
        OP_FRAME     = 1,
        OP_CLOSE     = 2,
        OP_PING      = 3,
        OP_PONG      = 4,
    };

    struct Header
    {
        uint32_t opcode;
        uint32_t length;
    };

    bool     writeFrame(Opcode opcode, const QByteArray& payload);
    bool     readFrame(Opcode& outOpcode, QByteArray& outPayload);
    QByteArray socketPath() const;

    // Build a minimal JSON payload for SET_ACTIVITY.
    // We hand-roll this to avoid any JSON library dependency.
    static QByteArray buildSetActivityPayload(
        const char* details,
        const char* state,
        const char* largeImageKey,
        const char* largeImageText,
        const char* smallImageKey,
        const char* smallImageText,
        int64_t     startTimestamp,
        int         nonce);

    static QByteArray buildClearPayload(int nonce);

    // Escape a plain ASCII string for embedding in JSON.
    static QByteArray jsonEscape(const char* str);

    int m_fd    = -1;
    int m_nonce = 1;
};
