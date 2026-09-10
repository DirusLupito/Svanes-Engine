#pragma once

#include <cstdint>
#include <string>

constexpr const char* kChrisDefaultServerHost = "127.0.0.1";
constexpr std::uint16_t kChrisStatePort = 5555;
constexpr std::uint16_t kChrisInputPort = 5557;

inline std::string ChrisServerEndpoint(const std::string& host, std::uint16_t port)
{
    return "tcp://" + host + ":" + std::to_string(port);
}

// Level layout shared by the server, which is authoritative for collision, and the client,
// which renders it. Both sides must agree on these exactly or the character will appear to
// collide with geometry that isn't where it's drawn.

constexpr float kChrisWorldWidth = 1920.0F;
constexpr float kChrisWorldHeight = 1080.0F;

constexpr float kChrisGroundHeight = 64.0F;

constexpr float kChrisPlatformWidth = 300.0F;
constexpr float kChrisPlatformHeight = 40.0F;
constexpr float kChrisPlatformLeft = 500.0F;
constexpr float kChrisPlatformTopHeightAboveGround = 260.0F;

constexpr float kChrisCharacterWidth = 180.0F;
constexpr float kChrisCharacterHeight = 200.0F;

constexpr float kChrisShapeSize = 96.0F;

constexpr float kChrisBeatSyncIntervalSeconds = 0.25F;

/**
 * Sent by a client every frame to report its current directional input.
 * The server decides which entity this affects based on the sender's assigned role.
 *
 * FIELDS:
 * - client_id: A value the client randomly generates once at startup to identify itself to the server.
 * - horizontal: Horizontal input axis, from -1 (left) to 1 (right).
 * - vertical: Vertical input axis, from -1 (up) to 1 (down). Only meaningful for the shape role.
 * - jump_requested: True if the jump key was pressed since the last input message was sent.
 *   Only meaningful for the character role, and dropped by the server if not grounded when received.
 */
struct PlayerInputMessage {
    std::uint32_t client_id;
    float horizontal;
    float vertical;
    bool jump_requested;
};

/**
 * Broadcast by the server so every client renders both entities in the same place.
 *
 * FIELDS:
 * - character_x: World x position of the character entity.
 * - character_y: World y position of the character entity.
 * - shape_x: World x position of the shape entity.
 * - shape_y: World y position of the shape entity.
 */
struct GameStateMessage {
    float character_x;
    float character_y;
    float shape_x;
    float shape_y;
};

/**
 * Broadcast by the server every kChrisBeatSyncIntervalSeconds so clients can anchor their own
 * beat clock to the server's and, on every message after the first, check how far their beat
 * clock has drifted from the server's. The server's beat clock starts when the server does.
 *
 * FIELDS:
 * - elapsed_beat_seconds: Seconds the server's beat clock had been running when this was sent.
 */
struct BeatSyncMessage {
    float elapsed_beat_seconds;
};
