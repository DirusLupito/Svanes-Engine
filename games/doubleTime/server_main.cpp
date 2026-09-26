#include "tilemap_parser.hpp"
#include "network_protocol.hpp"

#include <svanes/async/async_parallel_for_driver.hpp>
#include <svanes/collision_system.hpp>
#include <svanes/kinematic_system.hpp>
#include <svanes/network/network_replication.hpp>
#include <svanes/network/network_server.hpp>
#include <svanes/network/udp_msg_pipe.hpp>
#include <svanes/physics_system.hpp>
#include <svanes/registry.hpp>
#include <svanes/network/server_runtime.hpp>
#include <svanes/timeline_system.hpp>
#include <svanes/vector2d.hpp>

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <variant>

namespace {

constexpr float kCharacterMoveSpeedPerSecond = 360.0F * kChrisWorldScale;
constexpr float kJumpSpeedPerSecond = 800.0F * kChrisWorldScale;
constexpr float kGravityPerSecondSquared = 1500.0F * kChrisWorldScale;

constexpr float kCharacterSpawnLeft = 20.0F;
constexpr float kCharacterSpawnTop = 20.0F;
constexpr float kCharacterColliderLeftInset = 10.0F * 0.75F;
constexpr float kCharacterColliderRightInset = 11.0F * 0.75F;
constexpr float kCharacterColliderWidth = kChrisCharacterWidth -
                                          kCharacterColliderLeftInset -
                                          kCharacterColliderRightInset;
constexpr float kCharacterColliderOffsetX =
    (kCharacterColliderLeftInset - kCharacterColliderRightInset) * 0.5F;

constexpr std::int32_t kMaxResolutionIterations = 4;

constexpr float kPlatformSlideDistance = 360.0F * kChrisWorldScale;
constexpr float kPlatformSlideSpeedPerSecond = 1400.0F * kChrisWorldScale;
constexpr float kPlatformBeatSeconds = 0.5F;
constexpr std::int32_t kPlatformHoldBeats = 2;

struct ServerCharacter {
    svanes::Entity entity;
    svanes::ClientId client_id;
    float horizontal_input = 0.0F;
    bool jump_requested = false;
    bool grounded_on_platform = false;
};

enum class PlatformState : std::uint8_t {
    Idle,
    SlidingLeft,
    Holding,
    SlidingBack,
};

struct ServerPlatform {
    svanes::Entity entity;
    float home_x = 0.0F;
    float target_left_x = 0.0F;
    PlatformState state = PlatformState::Idle;
    svanes::TicCount holding_tics_remaining = 0;
};

void ResolveCharacterAxis(svanes::Registry &world,
                          svanes::Entity character_entity, bool horizontal) {
    svanes::Transform &character_transform =
        world.GetComponent<svanes::Transform>(character_entity);
    const svanes::Collider2D &character_collider =
        world.GetComponent<svanes::Collider2D>(character_entity);
    svanes::Kinematic2D &character_motion =
        world.GetComponent<svanes::Kinematic2D>(character_entity);

    for (std::int32_t iteration = 0; iteration < kMaxResolutionIterations;
         ++iteration) {
        std::optional<svanes::Collision2D> deepest_collision;
        const auto consider_collision =
            [&](const svanes::Collision2D &collision) {
                const bool is_axis_aligned = horizontal
                                                 ? collision.normal.y == 0.0F
                                                 : collision.normal.x == 0.0F;
                if (!is_axis_aligned) {
                    return;
                }

                if (!deepest_collision.has_value() ||
                    collision.penetration_depth >
                        deepest_collision->penetration_depth) {
                    deepest_collision = collision;
                }
            };

        world.ForEach<svanes::Transform, svanes::Collider2D>(
            [&](svanes::Entity candidate_entity,
                const svanes::Transform &candidate_transform,
                const svanes::Collider2D &candidate_collider) {
                if (candidate_entity == character_entity) {
                    return;
                }

                for (const svanes::Collision2D &collision :
                     svanes::DetectCollisions(
                         character_collider.geometry, character_transform,
                         candidate_collider.geometry, candidate_transform)) {
                    consider_collision(collision);
                }
            });

        world.ForEach<svanes::Transform, svanes::TileMap>(
            [&](svanes::Entity, const svanes::Transform &tile_map_transform,
                const svanes::TileMap &tile_map) {
                for (const svanes::Collision2D &collision :
                     svanes::DetectTileMapCollisions(
                         character_collider.geometry, character_transform,
                         tile_map, tile_map_transform)) {
                    consider_collision(collision);
                }
            });

        if (!deepest_collision.has_value()) {
            break;
        }

        if (horizontal) {
            character_transform.x += deepest_collision->normal.x *
                                     deepest_collision->penetration_depth;
            character_motion.velocity_x = 0.0F;
        } else {
            character_transform.y += deepest_collision->normal.y *
                                     deepest_collision->penetration_depth;
            character_motion.velocity_y = 0.0F;
        }
    }
}

bool IsStandingOnPlatform(const svanes::Registry &world,
                          svanes::Entity character_entity,
                          svanes::Entity platform_entity) {
    const svanes::Transform &character_transform =
        world.GetComponent<svanes::Transform>(character_entity);
    const svanes::Collider2D &character_collider =
        world.GetComponent<svanes::Collider2D>(character_entity);
    const svanes::Kinematic2D &character_motion =
        world.GetComponent<svanes::Kinematic2D>(character_entity);
    const svanes::Rectangle2D &character_geometry =
        std::get<svanes::Rectangle2D>(character_collider.geometry);
    const svanes::Transform &platform_transform =
        world.GetComponent<svanes::Transform>(platform_entity);
    const svanes::Collider2D &platform_collider =
        world.GetComponent<svanes::Collider2D>(platform_entity);
    const svanes::Rectangle2D &platform_geometry =
        std::get<svanes::Rectangle2D>(platform_collider.geometry);

    const float character_left = character_transform.x + character_geometry.x -
                                 character_geometry.width * 0.5F;
    const float character_right = character_transform.x + character_geometry.x +
                                  character_geometry.width * 0.5F;
    const float character_bottom = character_transform.y +
                                   character_geometry.y +
                                   character_geometry.height * 0.5F;
    const float platform_left = platform_transform.x + platform_geometry.x -
                                platform_geometry.width * 0.5F;
    const float platform_right = platform_transform.x + platform_geometry.x +
                                 platform_geometry.width * 0.5F;
    const float platform_top = platform_transform.y + platform_geometry.y -
                               platform_geometry.height * 0.5F;
    constexpr float contact_tolerance = 0.1F;

    return character_motion.velocity_y == 0.0F &&
           std::fabs(character_bottom - platform_top) <= contact_tolerance &&
           character_right > platform_left && character_left < platform_right;
}

svanes::Entity SpawnCharacter(svanes::Registry &world, float x, float y) {
    const svanes::Entity entity = world.CreateEntity();
    world.AddComponent<svanes::Transform>(entity,
                                          svanes::Transform{x, y, 0.0F});
    world.AddComponent<svanes::Kinematic2D>(entity, svanes::Kinematic2D{});
    world.AddComponent<svanes::Gravity>(entity, svanes::Gravity{});
    world.AddComponent<svanes::Collider2D>(
        entity, svanes::Collider2D{svanes::Rectangle2D{
                    kCharacterColliderOffsetX, 0.0F, kCharacterColliderWidth,
                    kChrisCharacterHeight}});
    world.AddComponent<svanes::Timeline>(entity);
    world.AddComponent<svanes::Networked>(entity);
    return entity;
}

void AdvancePlatform(svanes::Registry &world, ServerPlatform &platform,
                     bool trigger_requested, svanes::TicCount hold_tics) {
    svanes::Transform &transform =
        world.GetComponent<svanes::Transform>(platform.entity);
    svanes::Kinematic2D &motion =
        world.GetComponent<svanes::Kinematic2D>(platform.entity);

    switch (platform.state) {
    case PlatformState::Idle:
        if (trigger_requested) {
            platform.state = PlatformState::SlidingLeft;
            motion.velocity_x =
                -svanes::PerSecondToPerTic(kPlatformSlideSpeedPerSecond);
        }
        break;

    case PlatformState::SlidingLeft:
        if (transform.x <= platform.target_left_x) {
            transform.x = platform.target_left_x;
            motion.velocity_x = 0.0F;
            platform.state = PlatformState::Holding;
            platform.holding_tics_remaining = hold_tics;
        }
        break;

    case PlatformState::Holding:
        platform.holding_tics_remaining -= std::min(
            platform.holding_tics_remaining, svanes::DefaultPhysicsStepTics);
        if (platform.holding_tics_remaining == 0) {
            platform.state = PlatformState::SlidingBack;
            motion.velocity_x =
                svanes::PerSecondToPerTic(kPlatformSlideSpeedPerSecond);
        }
        break;

    case PlatformState::SlidingBack:
        if (transform.x >= platform.home_x) {
            transform.x = platform.home_x;
            motion.velocity_x = 0.0F;
            platform.state = PlatformState::Idle;
        }
        break;
    }
}

} // namespace

int32_t main() {
    svanes::NetworkServer network_server(
        std::make_unique<svanes::UdpMsgPipe>(kChrisServerPort));

    svanes::Registry world;

    const float ground_top = kChrisWorldHeight - kChrisGroundHeight;
    const float platform_top = ground_top - kChrisPlatformTopHeightAboveGround;
    const float platform_home_x =
        kChrisPlatformLeft + kChrisPlatformWidth * 0.5F;

    const svanes::Entity platform_entity = world.CreateEntity();
    if (platform_entity != kChrisPlatformNetworkEntity) {
        SDL_Log("Server: expected the platform to be entity %u but entity "
                "creation order produced %u instead.",
                kChrisPlatformNetworkEntity, platform_entity);
        return 1;
    }
    world.AddComponent<svanes::Transform>(
        platform_entity,
        svanes::Transform{platform_home_x,
                          platform_top + kChrisPlatformHeight * 0.5F, 0.0F});
    world.AddComponent<svanes::Collider2D>(
        platform_entity,
        svanes::Collider2D{svanes::Rectangle2D{0.0F, 0.0F, kChrisPlatformWidth,
                                               kChrisPlatformHeight}});
    world.AddComponent<svanes::Kinematic2D>(platform_entity,
                                            svanes::Kinematic2D{});
    world.AddComponent<svanes::Timeline>(platform_entity);
    world.AddComponent<svanes::Networked>(platform_entity);

    ServerPlatform platform{platform_entity, platform_home_x,
                            platform_home_x - kPlatformSlideDistance};
    const svanes::TicCount platform_hold_tics =
        svanes::SecondsToTics(kPlatformBeatSeconds * kPlatformHoldBeats);

    const svanes::Entity ground_entity = world.CreateEntity();
    svanes::TileMap tile_map = CreateDoubleTimeTileMapFromPng(
        std::string{DOUBLE_TIME_GAME_ASSETS_DIR} + "/" +
        kChrisTileMapFilename);
    world.AddComponent<svanes::Transform>(
        ground_entity,
        GetCenteredTileMapTransform(tile_map, kChrisWorldWidth * 0.5F,
                                    kChrisWorldHeight * 0.5F));
    world.AddComponent<svanes::TileMap>(ground_entity, std::move(tile_map));

    std::optional<ServerCharacter> character;
    bool platform_trigger_requested = false;

    const svanes::Vector2D gravity{
        0.0F,
        svanes::PerSecondSquaredToPerTicSquared(kGravityPerSecondSquared)};

    svanes::ServerRuntime runtime(network_server, world, gravity);
    float character_start_y = 0.0F;
    svanes::Transform platform_start_transform{};
    runtime.Run(
        [&](const svanes::NetworkMessage &message) {
            const PlayerInputMessage input = message.As<PlayerInputMessage>();

            if (input.role == ClientRole::Platform) {
                platform_trigger_requested =
                    platform_trigger_requested || input.action_requested;
                return;
            }

            if (!character.has_value()) {
                const svanes::Entity entity = SpawnCharacter(
                    world, kCharacterSpawnLeft, kCharacterSpawnTop);
                character = ServerCharacter{entity, input.client_id};
                SDL_Log("Server: spawned the character for client %u.",
                        input.client_id);
            } else if (character->client_id != input.client_id) {
                SDL_Log("Server: client %u took control of the character from "
                        "client %u.",
                        input.client_id, character->client_id);
                character->client_id = input.client_id;
            }

            character->horizontal_input = input.horizontal;
            character->jump_requested =
                character->jump_requested || input.action_requested;
        },
        [&]() {
            if (character.has_value()) {
                svanes::Kinematic2D &motion =
                    world.GetComponent<svanes::Kinematic2D>(character->entity);
                motion.velocity_x =
                    character->horizontal_input *
                    svanes::PerSecondToPerTic(kCharacterMoveSpeedPerSecond);
                character_start_y =
                    world.GetComponent<svanes::Transform>(character->entity).y;
            }
            platform_start_transform =
                world.GetComponent<svanes::Transform>(platform.entity);
        },
        [&](std::span<const svanes::PhysicsTimeStep>) {
                    AdvancePlatform(world, platform, platform_trigger_requested,
                                    platform_hold_tics);
                    platform_trigger_requested = false;

                    const svanes::Transform &platform_transform =
                        world.GetComponent<svanes::Transform>(platform.entity);
                    const float platform_delta_x =
                        platform_transform.x - platform_start_transform.x;
                    const float platform_delta_y =
                        platform_transform.y - platform_start_transform.y;

                    if (character.has_value()) {
                        svanes::Transform &transform =
                            world.GetComponent<svanes::Transform>(
                                character->entity);
                        const float carry_x = character->grounded_on_platform
                                                  ? platform_delta_x
                                                  : 0.0F;
                        const float carry_y = character->grounded_on_platform
                                                  ? platform_delta_y
                                                  : 0.0F;
                        const float target_x = std::clamp(
                            transform.x + carry_x, 0.0F, kChrisWorldWidth);
                        const float target_y = std::clamp(
                            transform.y + carry_y, 0.0F, kChrisWorldHeight);
                        transform.x = target_x;
                        transform.y = std::clamp(character_start_y + carry_y,
                                                 0.0F, kChrisWorldHeight);
                        ResolveCharacterAxis(world, character->entity, true);
                        transform.y = target_y;
                        ResolveCharacterAxis(world, character->entity, false);

                        svanes::Kinematic2D &motion =
                            world.GetComponent<svanes::Kinematic2D>(
                                character->entity);
                        const bool is_grounded = motion.velocity_y == 0.0F;
                        character->grounded_on_platform = IsStandingOnPlatform(
                            world, character->entity, platform.entity);
                        if (is_grounded && character->jump_requested) {
                            motion.velocity_y =
                                -svanes::PerSecondToPerTic(kJumpSpeedPerSecond);
                            character->grounded_on_platform = false;
                        }
                        character->jump_requested = false;
                    }
        },
        [&]() {
            for (const svanes::EntityTransformState &state :
                 svanes::CollectTransformStates(world)) {
                const bool movement_key_held =
                    character.has_value() &&
                    state.network_entity == character->entity &&
                    character->horizontal_input != 0.0F;
                network_server.Broadcast(DoubleTimeEntityState{
                    state.network_entity, state.transform, movement_key_held});
            }
        });
}
