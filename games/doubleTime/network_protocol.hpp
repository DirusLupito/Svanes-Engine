#pragma once

#include <svanes/entity.hpp>
#include <svanes/geometry/geometry.hpp>
#include <svanes/network/network_message.hpp>

#include <cstdint>

constexpr const char *kChrisDefaultServerHost = "127.0.0.1";
constexpr std::uint16_t kChrisServerPort = 5555;
constexpr const char *kChrisTileMapFilename = "TestTilemap.png";
constexpr const char *kChrisMusicFilename = "audio/music/OldCity.wav";

constexpr float kChrisTileSize = 10.0F;
constexpr std::uint32_t kChrisVisibleTileRows = 24;
constexpr float kChrisWorldHeight =
    kChrisTileSize * static_cast<float>(kChrisVisibleTileRows);
constexpr float kChrisWorldWidth = kChrisWorldHeight * (16.0F / 9.0F);
constexpr float kChrisWorldScale = kChrisWorldHeight / 1080.0F;

constexpr float kChrisGroundHeight = 20.0F;

constexpr float kChrisPlatformWidth = 90.0F;
constexpr float kChrisPlatformHeight = 10.0F;
constexpr float kChrisPlatformLeft = 150.0F;
constexpr float kChrisPlatformTopHeightAboveGround = 80.0F;

constexpr float kChrisCharacterHeight = 3.0F * kChrisTileSize;
constexpr float kChrisCharacterWidth =
    kChrisCharacterHeight * (288.0F / 320.0F);

constexpr svanes::Entity kChrisPlatformNetworkEntity = 0;

enum class ClientRole : std::uint8_t {
    Character,
    Platform,
};

struct PlayerInputMessage {
    svanes::ClientId client_id;
    ClientRole role;
    float horizontal;
    bool action_requested;
};

struct DoubleTimeEntityState {
    svanes::Entity network_entity;
    svanes::Transform transform;
    bool movement_key_held = false;
};
