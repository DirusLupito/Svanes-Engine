#pragma once

#include <svanes/network/network_message.hpp>

#include <cstdint>

constexpr const char *kChrisDefaultServerHost = "127.0.0.1";
constexpr std::uint16_t kChrisStatePort = 5555;
constexpr std::uint16_t kChrisInputPort = 5557;

constexpr float kChrisWorldWidth = 1920.0F;
constexpr float kChrisWorldHeight = 1080.0F;

constexpr float kChrisGroundHeight = 64.0F;

constexpr float kChrisPlatformWidth = 300.0F;
constexpr float kChrisPlatformHeight = 40.0F;
constexpr float kChrisPlatformLeft = 500.0F;
constexpr float kChrisPlatformTopHeightAboveGround = 260.0F;

constexpr float kChrisCharacterWidth = 96.0F;
constexpr float kChrisCharacterHeight = 128.0F;

struct PlayerInputMessage {
    svanes::ClientId client_id;
    float horizontal;
    bool jump_requested;
};
