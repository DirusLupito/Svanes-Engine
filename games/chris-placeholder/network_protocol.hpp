#pragma once

#include <svanes/entity.hpp>
#include <svanes/network/network_message.hpp>

#include <cstdint>

constexpr const char *kChrisDefaultServerHost = "127.0.0.1";
constexpr std::uint16_t kChrisServerPort = 5555;

constexpr float kChrisWorldWidth = 1920.0F;
constexpr float kChrisWorldHeight = 1080.0F;

constexpr float kChrisGroundHeight = 64.0F;

constexpr float kChrisPlatformWidth = 300.0F;
constexpr float kChrisPlatformHeight = 40.0F;
constexpr float kChrisPlatformLeft = 500.0F;
constexpr float kChrisPlatformTopHeightAboveGround = 260.0F;

constexpr float kChrisCharacterWidth = 96.0F;
constexpr float kChrisCharacterHeight = 128.0F;

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
