#include "chris_game.hpp"

#include <svanes/camera2d.hpp>
#include <svanes/input.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>
#include <svanes/timeline_system.hpp>

#include <SDL3/SDL.h>

#include <array>
#include <cstdint>

namespace {

constexpr float kInputSendIntervalSeconds = 1.0F / 60.0F;

constexpr std::int32_t kGroundZOrder = 1;
constexpr std::int32_t kCharacterZOrder = 2;

constexpr svanes::Color kBackgroundColor{17, 24, 39, 255};
constexpr svanes::Color kGroundColor{92, 64, 51, 255};
constexpr svanes::Color kPlatformColor{160, 82, 45, 255};

constexpr std::array<svanes::Color, 4> kCharacterPalette{
    svanes::Color{37, 99, 235, 255},
    svanes::Color{220, 38, 38, 255},
    svanes::Color{22, 163, 74, 255},
    svanes::Color{234, 179, 8, 255},
};

} // namespace

ChrisGame::ChrisGame(std::string server_host)
    : network_client(server_host, kChrisStatePort, kChrisInputPort) {
    SDL_Log("Networking: client %u connecting to server at %s.", network_client.Id(), server_host.c_str());
}

void ChrisGame::Initialize(svanes::GameContext &context) {
    context.camera.scale_mode = svanes::ScaleMode::Proportional;

    const svanes::Entity background_entity = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(
        background_entity, svanes::Transform{kChrisWorldWidth * 0.5F, kChrisWorldHeight * 0.5F, 0.0F}
    );
    context.world.AddComponent<svanes::SolidShape>(
        background_entity,
        svanes::SolidShape{kBackgroundColor, svanes::Rectangle2D{0.0F, 0.0F, kChrisWorldWidth, kChrisWorldHeight}}
    );

    const float ground_top = kChrisWorldHeight - kChrisGroundHeight;
    const svanes::Entity ground_entity = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(
        ground_entity, svanes::Transform{kChrisWorldWidth * 0.5F, ground_top + kChrisGroundHeight * 0.5F, 0.0F}
    );
    context.world.AddComponent<svanes::SolidShape>(
        ground_entity,
        svanes::SolidShape{kGroundColor, svanes::Rectangle2D{0.0F, 0.0F, kChrisWorldWidth, kChrisGroundHeight}}
    );
    context.world.AddComponent<svanes::ZOrder>(ground_entity, svanes::ZOrder{kGroundZOrder});

    const float platform_top = ground_top - kChrisPlatformTopHeightAboveGround;
    const svanes::Entity platform_entity = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(
        platform_entity,
        svanes::Transform{
            kChrisPlatformLeft + kChrisPlatformWidth * 0.5F, platform_top + kChrisPlatformHeight * 0.5F, 0.0F
        }
    );
    context.world.AddComponent<svanes::SolidShape>(
        platform_entity,
        svanes::SolidShape{kPlatformColor, svanes::Rectangle2D{0.0F, 0.0F, kChrisPlatformWidth, kChrisPlatformHeight}}
    );
    context.world.AddComponent<svanes::ZOrder>(platform_entity, svanes::ZOrder{kGroundZOrder});
}

svanes::Entity ChrisGame::SpawnRemoteCharacter(svanes::Registry &world, svanes::Entity network_entity) {
    SDL_Log("Networking: client %u observed new networked entity %u.", network_client.Id(), network_entity);

    const svanes::Entity entity = world.CreateEntity();
    world.AddComponent<svanes::Transform>(entity, svanes::Transform{});
    world.AddComponent<svanes::SolidShape>(
        entity,
        svanes::SolidShape{
            kCharacterPalette[network_entity % kCharacterPalette.size()],
            svanes::Rectangle2D{0.0F, 0.0F, kChrisCharacterWidth, kChrisCharacterHeight},
        }
    );
    world.AddComponent<svanes::ZOrder>(entity, svanes::ZOrder{kCharacterZOrder});
    return entity;
}

void ChrisGame::SendInput(const svanes::FrameContext &frame) {
    input_send_timer += static_cast<float>(frame.real_delta_tics) / static_cast<float>(svanes::TicsPerSecond);
    if (input_send_timer < kInputSendIntervalSeconds) {
        return;
    }
    input_send_timer -= kInputSendIntervalSeconds;

    float horizontal_input = 0.0F;
    if (frame.input.IsDown(svanes::Key::Left)) {
        horizontal_input -= 1.0F;
    }
    if (frame.input.IsDown(svanes::Key::Right)) {
        horizontal_input += 1.0F;
    }

    const PlayerInputMessage input{network_client.Id(), horizontal_input, jump_requested_since_last_send};
    network_client.Send(input);
    jump_requested_since_last_send = false;
}

void ChrisGame::ApplyServerState(svanes::Registry &world) {
    for (const svanes::NetworkMessage &message : network_client.PollBroadcast()) {
        const svanes::EntityTransformState state = message.As<svanes::EntityTransformState>();
        svanes::ApplyTransformState(world, entity_map, state, [&]() {
            return SpawnRemoteCharacter(world, state.network_entity);
        });
    }
}

void ChrisGame::Update(const svanes::FrameContext &frame) {
    if (frame.input.WasPressed(svanes::Key::Space)) {
        jump_requested_since_last_send = true;
    }

    SendInput(frame);
    ApplyServerState(frame.world);

    frame.camera.x = kChrisWorldWidth * 0.5F;
    frame.camera.y = kChrisWorldHeight * 0.5F;
}
