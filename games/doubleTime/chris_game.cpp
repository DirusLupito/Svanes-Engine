#include "chris_game.hpp"
#include "tilemap_parser.hpp"

#include <svanes/camera2d.hpp>
#include <svanes/input.hpp>
#include <svanes/network/udp_msg_pipe.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>
#include <svanes/render/texture_manager.hpp>
#include <svanes/sprite_animation_system.hpp>
#include <svanes/timeline_system.hpp>

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdint>
#include <memory>

namespace {

constexpr float kInputSendIntervalSeconds = 1.0F / 60.0F;

constexpr std::int32_t kGroundZOrder = 1;
constexpr std::int32_t kCharacterZOrder = 2;

constexpr svanes::Color kBackgroundColor{17, 24, 39, 255};
constexpr svanes::Color kPlatformColor{160, 82, 45, 255};

constexpr std::int32_t kCharacterFrameWidth = 288;
constexpr std::int32_t kCharacterFrameHeight = 320;
constexpr std::int32_t kIdleFrameCount = 8;
constexpr std::int32_t kRunningFrameCount = 10;
constexpr float kCharacterFramesPerSecond = 12.0F;
constexpr float kCharacterRunPositionEpsilon = 0.5F;

constexpr const char *kIdleSpriteSheetFilename = "sheet_umeko-idle.png";
constexpr const char *kRunningSpriteSheetFilename = "sheet_umeko-run.png";
constexpr const char *kTestTileSetFilename = "TestTileSet.png";

constexpr float kNetworkSmoothingRatePerSecond = 25.0F;

struct NetworkInterpolationTarget {
    svanes::Transform target;
};

} // namespace

ChrisGame::ChrisGame(std::string server_host, ClientRole role)
    : network_client(std::make_unique<svanes::UdpMsgPipe>(0, server_host,
                                                          kChrisServerPort)),
      role(role) {
    SDL_Log("Networking: client %u connecting to server at %s.",
            network_client.Id(), server_host.c_str());
}

void ChrisGame::Initialize(svanes::GameContext &context) {
    context.camera.scale_mode = svanes::ScaleMode::Proportional;
    context.camera.zoom = context.camera.Viewport().height /
                          context.camera.Scale() / kChrisWorldHeight;

    const svanes::Entity background_entity = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(
        background_entity, svanes::Transform{kChrisWorldWidth * 0.5F,
                                             kChrisWorldHeight * 0.5F, 0.0F});
    context.world.AddComponent<svanes::SolidShape>(
        background_entity,
        svanes::SolidShape{kBackgroundColor,
                           svanes::Rectangle2D{0.0F, 0.0F, kChrisWorldWidth,
                                               kChrisWorldHeight}});

    idle_texture =
        context.assets.LoadTexture(std::string{DOUBLE_TIME_GAME_ASSETS_DIR} +
                                   "/" + kIdleSpriteSheetFilename);
    running_texture =
        context.assets.LoadTexture(std::string{DOUBLE_TIME_GAME_ASSETS_DIR} +
                                   "/" + kRunningSpriteSheetFilename);

    const svanes::TextureHandle test_tile_set = context.assets.LoadTexture(
        std::string{DOUBLE_TIME_GAME_ASSETS_DIR} + "/" + kTestTileSetFilename);
    const svanes::Entity tile_map_entity = context.world.CreateEntity();
    svanes::TileMap tile_map = CreateDoubleTimeTileMapFromPng(
        std::string{DOUBLE_TIME_GAME_ASSETS_DIR} + "/" +
            kChrisTileMapFilename,
        test_tile_set);
    context.world.AddComponent<svanes::Transform>(
        tile_map_entity,
        GetCenteredTileMapTransform(tile_map, kChrisWorldWidth * 0.5F,
                                    kChrisWorldHeight * 0.5F));
    context.world.AddComponent<svanes::TileMap>(
        tile_map_entity, std::move(tile_map));
    context.world.AddComponent<svanes::ZOrder>(tile_map_entity,
                                               svanes::ZOrder{kGroundZOrder});
}

svanes::Entity ChrisGame::SpawnCharacter(svanes::Registry &world,
                                         svanes::Transform initial_transform) {
    SDL_Log("Networking: client %u observed the networked character.",
            network_client.Id());

    const svanes::Entity entity = world.CreateEntity();
    world.AddComponent<svanes::Transform>(entity, initial_transform);
    world.AddComponent<svanes::Sprite>(
        entity,
        svanes::Sprite{
            .texture = idle_texture,
            .geometry = svanes::Rectangle2D{0.0F, 0.0F, kChrisCharacterWidth,
                                            kChrisCharacterHeight},
        });
    world.AddComponent<svanes::SpriteAnimation>(
        entity, svanes::SpriteAnimation{
                    .frame_width = kCharacterFrameWidth,
                    .frame_height = kCharacterFrameHeight,
                    .frame_count = kIdleFrameCount,
                    .tics_per_frame =
                        svanes::SecondsToTics(1.0 / kCharacterFramesPerSecond),
                });
    world.AddComponent<svanes::Timeline>(entity);
    world.AddComponent<svanes::ZOrder>(entity,
                                       svanes::ZOrder{kCharacterZOrder});
    character_is_running = false;
    return entity;
}

svanes::Entity ChrisGame::SpawnPlatform(svanes::Registry &world,
                                        svanes::Transform initial_transform) {
    SDL_Log("Networking: client %u observed the networked platform.",
            network_client.Id());

    const svanes::Entity entity = world.CreateEntity();
    world.AddComponent<svanes::Transform>(entity, initial_transform);
    world.AddComponent<svanes::SolidShape>(
        entity,
        svanes::SolidShape{kPlatformColor,
                           svanes::Rectangle2D{0.0F, 0.0F, kChrisPlatformWidth,
                                               kChrisPlatformHeight}});
    world.AddComponent<svanes::ZOrder>(entity, svanes::ZOrder{kGroundZOrder});
    return entity;
}

void ChrisGame::SendInput(const svanes::FrameContext &frame) {
    input_send_timer += static_cast<float>(frame.real_delta_tics) /
                        static_cast<float>(svanes::TicsPerSecond);
    if (input_send_timer < kInputSendIntervalSeconds) {
        return;
    }
    input_send_timer -= kInputSendIntervalSeconds;

    float horizontal_input = 0.0F;
    if (role == ClientRole::Character) {
        if (frame.input.IsDown(svanes::Key::Left)) {
            horizontal_input -= 1.0F;
        }
        if (frame.input.IsDown(svanes::Key::Right)) {
            horizontal_input += 1.0F;
        }
    }

    const PlayerInputMessage input{network_client.Id(), role, horizontal_input,
                                   action_requested_since_last_send};
    network_client.Send(input);
    action_requested_since_last_send = false;
}

void ChrisGame::ApplyServerState(svanes::Registry &world) {
    for (const svanes::NetworkMessage &message :
         network_client.PollBroadcast()) {
        const DoubleTimeEntityState state = message.As<DoubleTimeEntityState>();

        if (state.network_entity == kChrisPlatformNetworkEntity) {
            const svanes::Entity local_entity =
                entity_map.Resolve(state.network_entity, [&]() {
                    return SpawnPlatform(world, state.transform);
                });
            world.AddComponent<NetworkInterpolationTarget>(
                local_entity, NetworkInterpolationTarget{state.transform});
            continue;
        }

        bool freshly_spawned = false;
        const svanes::Entity local_entity =
            entity_map.Resolve(state.network_entity, [&]() {
                freshly_spawned = true;
                return SpawnCharacter(world, state.transform);
            });

        const float previous_target_x =
            freshly_spawned
                ? state.transform.x
                : world.GetComponent<NetworkInterpolationTarget>(local_entity)
                      .target.x;
        world.AddComponent<NetworkInterpolationTarget>(
            local_entity, NetworkInterpolationTarget{state.transform});

        const bool is_running =
            state.movement_key_held &&
            std::fabs(state.transform.x - previous_target_x) >
                kCharacterRunPositionEpsilon;
        if (is_running != character_is_running) {
            character_is_running = is_running;

            svanes::Sprite &sprite =
                world.GetComponent<svanes::Sprite>(local_entity);
            sprite.texture =
                character_is_running ? running_texture : idle_texture;

            svanes::SpriteAnimation &animation =
                world.GetComponent<svanes::SpriteAnimation>(local_entity);
            animation.frame_count =
                character_is_running ? kRunningFrameCount : kIdleFrameCount;
            animation.current_frame = 0;
            animation.elapsed_tics = 0;
        }
    }
}

void ChrisGame::SmoothNetworkedTransforms(const svanes::FrameContext &frame) {
    const float delta_seconds = static_cast<float>(frame.real_delta_tics) /
                                static_cast<float>(svanes::TicsPerSecond);
    const float smoothing =
        1.0F - std::exp(-kNetworkSmoothingRatePerSecond * delta_seconds);

    frame.world.ForEach<svanes::Transform, NetworkInterpolationTarget>(
        [&](svanes::Entity, svanes::Transform &transform,
            const NetworkInterpolationTarget &interpolation) {
            transform.x += (interpolation.target.x - transform.x) * smoothing;
            transform.y += (interpolation.target.y - transform.y) * smoothing;
            transform.rotation +=
                (interpolation.target.rotation - transform.rotation) *
                smoothing;
        });
}

void ChrisGame::Update(const svanes::FrameContext &frame) {
    if (frame.input.WasPressed(svanes::Key::Space)) {
        action_requested_since_last_send = true;
    }

    SendInput(frame);
    ApplyServerState(frame.world);
    SmoothNetworkedTransforms(frame);

    frame.camera.x = kChrisWorldWidth * 0.5F;
    frame.camera.y = kChrisWorldHeight * 0.5F;
}
