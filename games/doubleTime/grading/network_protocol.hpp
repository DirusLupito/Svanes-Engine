#pragma once

#include <svanes/entity.hpp>
#include <svanes/geometry/geometry.hpp>
#include <svanes/network/network_message.hpp>
#include <svanes/network/clientserver/network_server.hpp>

#include <cstdint>

constexpr const char *kDoubleTimeDefaultServerHost = "127.0.0.1";
constexpr std::uint16_t kDoubleTimeServerJoinPort = 5558;

constexpr float kDoubleTimeTileSize = 10.0F;
constexpr std::uint32_t kDoubleTimeVisibleTileRows = 24;
constexpr float kDoubleTimeWorldHeight =
    kDoubleTimeTileSize * static_cast<float>(kDoubleTimeVisibleTileRows);
constexpr float kDoubleTimeWorldWidth = kDoubleTimeWorldHeight * (16.0F / 9.0F);
constexpr float kDoubleTimeWorldScale = kDoubleTimeWorldHeight / 1080.0F;

constexpr float kDoubleTimeCharacterHeight = 0.9F * kDoubleTimeTileSize;
constexpr float kDoubleTimeCharacterWidth = 0.9F * kDoubleTimeTileSize;

enum class DoubleTimeNetworkingStrategy : std::uint8_t {
    InputAuthoritative,
    ClientPositionAuthoritative,
};

#if defined(DOUBLE_TIME_GRADING_CLIENT_POSITION)
constexpr DoubleTimeNetworkingStrategy kDoubleTimeNetworkingStrategy =
    DoubleTimeNetworkingStrategy::ClientPositionAuthoritative;
#else
constexpr DoubleTimeNetworkingStrategy kDoubleTimeNetworkingStrategy =
    DoubleTimeNetworkingStrategy::InputAuthoritative;
#endif

struct PlayerInputMessage {
    svanes::ClientId client_id;
    float horizontal;
    bool action_requested;
    bool jump_on_beat;
};

struct PlayerTransformMessage {
    svanes::ClientId client_id;
    svanes::Transform transform;
    svanes::Vector2D velocity;
};

enum class DoubleTimeEntityKind : std::uint8_t {
    Character,
    CharacterDespawn,
    MovingPlatform,
    AudioTimeline,
    Door,
    Spikes,
    Collectible,
};

struct DoubleTimeEntityState {
    svanes::Transform transform;
    bool movement_key_held = false;
    std::uint32_t entity_id = 0;
    DoubleTimeEntityKind kind = DoubleTimeEntityKind::Character;
    float width = 0.0F;
    float height = 0.0F;
    std::uint64_t timeline_tics = 0;
    bool enabled = true;
    std::uint32_t area_index = 0;
    std::uint32_t tilemap_index = 0;
};
