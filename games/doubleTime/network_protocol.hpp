#pragma once

#include <svanes/entity.hpp>
#include <svanes/geometry/geometry.hpp>
#include <svanes/network/network_message.hpp>

#include <cstdint>

constexpr const char *kDoubleTimeDefaultServerHost = "127.0.0.1";
constexpr std::uint16_t kDoubleTimeServerJoinPort = 5558;
constexpr std::uint16_t kDoubleTimeInputBridgePort = 5600;
constexpr std::uint16_t kDoubleTimeInputBridgeCharacterPort = 5601;
constexpr std::uint16_t kDoubleTimeInputBridgePlatformPort = 5602;

constexpr float kDoubleTimeTileSize = 10.0F;
constexpr std::uint32_t kDoubleTimeVisibleTileRows = 24;
constexpr float kDoubleTimeWorldHeight =
    kDoubleTimeTileSize * static_cast<float>(kDoubleTimeVisibleTileRows);
constexpr float kDoubleTimeWorldWidth = kDoubleTimeWorldHeight * (16.0F / 9.0F);
constexpr float kDoubleTimeWorldScale = kDoubleTimeWorldHeight / 1080.0F;

constexpr float kDoubleTimeCharacterHeight = 0.9F * kDoubleTimeTileSize;
constexpr float kDoubleTimeCharacterWidth = 0.9F * kDoubleTimeTileSize;

struct PlayerInputMessage {
    svanes::ClientId client_id;
    float horizontal;
    bool action_requested;
    bool jump_on_beat;
    bool platform_role;
    bool platform_left;
    bool platform_right;
    bool platform_up;
    bool platform_down;
    bool platform_action_requested;
    bool platform_left_on_beat;
    bool platform_right_on_beat;
    bool platform_up_on_beat;
    bool platform_down_on_beat;
    bool platform_action_on_beat;
    std::uint64_t platform_left_beat_index;
    std::uint64_t platform_right_beat_index;
    std::uint64_t platform_up_beat_index;
    std::uint64_t platform_down_beat_index;
    std::uint64_t platform_action_beat_index;
};

struct DoubleTimeBridgeInput {
    float character_horizontal = 0.0F;
    bool character_jump = false;
    bool platform_left = false;
    bool platform_right = false;
    bool platform_up = false;
    bool platform_down = false;
    bool platform_action = false;
};

enum class DoubleTimeEntityKind : std::uint8_t {
    Character,
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
