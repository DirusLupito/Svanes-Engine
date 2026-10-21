#include "tilemap_parser.hpp"
#include "network_protocol.hpp"
#include "asset_catalog.hpp"
#include "rhythm_helpers.hpp"

#include <svanes/async/async_parallel_for_driver.hpp>
#include <svanes/collision_system.hpp>
#include <svanes/kinematic_system.hpp>
#include <svanes/network/clientserver/IServer.hpp>
#include <svanes/network/clientserver/network_server.hpp>
#include <svanes/network/udp_msg_pipe.hpp>
#include <svanes/physics_system.hpp>
#include <svanes/registry.hpp>
#include <svanes/timeline_system.hpp>
#include <svanes/vector2d.hpp>

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>
#include <variant>
#include <vector>
#include <functional>

namespace {

constexpr float kCharacterMaxSpeedPerSecond = 547.0F * kDoubleTimeWorldScale;
constexpr float kCharacterAccelerationPerSecondSquared =
    2400.0F * kDoubleTimeWorldScale;
constexpr float kCharacterDecelerationPerSecondSquared =
    3200.0F * kDoubleTimeWorldScale;
constexpr float kJumpSpeedPerSecond = 1200.0F * kDoubleTimeWorldScale;
constexpr float kGravityPerSecondSquared = 3000.0F * kDoubleTimeWorldScale;
constexpr float kCoyoteTimeSeconds = 0.10F;

constexpr float kCharacterSpawnLeft = 20.0F;
constexpr float kCharacterSpawnTop = 20.0F;
constexpr float kCharacterColliderWidth = kDoubleTimeCharacterWidth * 0.8F;
constexpr float kCharacterColliderOffsetX = 0.0F;

constexpr std::int32_t kMaxResolutionIterations = 4;
constexpr float kPlatformTravelDurationSeconds = 0.12F;
constexpr float kPlatformMoveDurationSeconds = 1.5F;
constexpr float kDoorOpenDurationSeconds = 1.5F;
constexpr std::uint32_t kPlatformHoldBeats = 4;
constexpr float kOffBeatJumpForceMultiplier = 0.2F;

enum class PlatformMotionPhase {
    Idle,
    MovingToEnd,
    Holding,
    MovingToStart,
};

struct PlatformMotionTrack {
    PlatformMotionPhase phase = PlatformMotionPhase::Idle;
    float elapsed = 0.0F;
    float direction = 1.0F;
    bool nudge = false;
    std::optional<svanes::TicCount> return_tics;
};

struct RigidBody {};

struct ServerCharacter {
    svanes::Entity entity;
    svanes::ClientId client_id;
    float horizontal_input = 0.0F;
    bool jump_requested = false;
    bool jump_on_beat = false;
    float time_since_grounded = 0.0F;
};

struct ServerPlatform {
    DoubleTimeMovingPlatform definition;
    svanes::Entity entity;
    svanes::Transform start_transform;
    svanes::Transform end_transform;
    svanes::Transform previous_transform;
};

struct ServerDoor {
    DoubleTimeDoor definition;
    svanes::Entity entity;
    svanes::Transform transform;
    svanes::Transform base_transform;
    bool open = false;
    std::optional<svanes::TicCount> close_tics;
    float nudge_remaining_seconds = 0.0F;
};

struct ServerAreaObject {
    DoubleTimeLevelArea definition;
    svanes::Entity entity;
    bool enabled = true;
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

        world.ForEach<svanes::Transform, svanes::Collider2D, RigidBody>(
            [&](svanes::Entity candidate_entity,
                const svanes::Transform &candidate_transform,
                const svanes::Collider2D &candidate_collider,
                const RigidBody &) {
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

svanes::Entity SpawnCharacter(svanes::Registry &world, float x, float y) {
    const svanes::Entity entity = world.CreateEntity();
    world.AddComponent<svanes::Transform>(entity,
                                          svanes::Transform{x, y, 0.0F});
    world.AddComponent<svanes::Kinematic2D>(entity, svanes::Kinematic2D{});
    world.AddComponent<svanes::Gravity>(entity, svanes::Gravity{});
    world.AddComponent<svanes::Collider2D>(
        entity, svanes::Collider2D{svanes::Rectangle2D{
                    kCharacterColliderOffsetX, 0.0F, kCharacterColliderWidth,
                    kDoubleTimeCharacterHeight}});
    world.AddComponent<svanes::Timeline>(entity);
    return entity;
}

} // namespace

class DoubleTimeServerRuntime final : public svanes::IServer {
public:
    using InitializeCallback = std::function<void(svanes::ServerContext &)>;
    using InputCallback = std::function<void(const svanes::NetworkMessage &)>;
    using UpdateCallback = std::function<void()>;
    using PhysicsCallback =
        std::function<void(std::span<const svanes::PhysicsTimeStep>)>;

    DoubleTimeServerRuntime(InputCallback input_callback,
                            UpdateCallback update_callback,
                            PhysicsCallback physics_callback,
                            UpdateCallback post_physics_callback,
                            svanes::Vector2D gravity,
                            InitializeCallback initialize_callback)
        : input_callback(std::move(input_callback)),
          update_callback(std::move(update_callback)),
          physics_callback(std::move(physics_callback)),
          post_physics_callback(std::move(post_physics_callback)),
          gravity(gravity),
          initialize_callback(std::move(initialize_callback)) {}

    void Initialize(svanes::ServerContext &context) override {
        initialize_callback(context);
        context.gravity = gravity;
    }

    svanes::JoinResponse HandleJoinRequest(
        const svanes::ServerJoinContext &) override {
        return svanes::JoinResponse{true, svanes::NetworkMessage{}};
    }

    void Update(const svanes::ServerUpdateContext &context) override {
        for (const svanes::ClientInput &input : context.network.PollInputs()) {
            PlayerInputMessage decoded = input.message.As<PlayerInputMessage>();
            decoded.client_id = input.client;
            input_callback(svanes::NetworkMessage::From(decoded));
        }
        update_callback();
    }

    void PhysicsUpdate(const svanes::ServerPhysicsContext &context) override {
        physics_callback(context.entity_steps);
        post_physics_callback();
    }

private:
    InputCallback input_callback;
    UpdateCallback update_callback;
    PhysicsCallback physics_callback;
    UpdateCallback post_physics_callback;
    svanes::Vector2D gravity;
    InitializeCallback initialize_callback;
};

namespace {

} // namespace

int32_t main(int32_t argc, char **argv) {
    if (argc > 1) {
        throw std::invalid_argument("The Double Time server takes no arguments.");
    }
    svanes::NetworkServer network_server;
    svanes::Registry world;
    svanes::Registry *active_world = &world;
    const svanes::Entity authoritative_timeline_entity = active_world->CreateEntity();
    active_world->AddComponent<svanes::Timeline>(authoritative_timeline_entity);

    svanes::Entity level_tilemap_entity = active_world->CreateEntity();
    const DoubleTimeAssetCatalogEntry &starting_area = DoubleTimeAssetCatalog().front();
    DoubleTimeLevelData level = LoadDoubleTimeLevelData(
        std::string{DOUBLE_TIME_GAME_ASSETS_DIR} + "/" +
        std::string{starting_area.tilemap_filenames.front()});
    svanes::Transform tile_map_transform = GetCenteredTileMapTransform(
        level.tile_map, kDoubleTimeWorldWidth * 0.5F,
        kDoubleTimeWorldHeight * 0.5F);
    float character_spawn_x = level.spawn_point.has_value()
        ? tile_map_transform.x +
              (level.spawn_point->x + 0.5F) * kDoubleTimeTileSize
        : kCharacterSpawnLeft;
    float character_spawn_y = level.spawn_point.has_value()
        ? tile_map_transform.y +
              (level.spawn_point->y + 0.5F) * kDoubleTimeTileSize
        : kCharacterSpawnTop;
    active_world->AddComponent<svanes::Transform>(level_tilemap_entity,
                                          tile_map_transform);
    active_world->AddComponent<svanes::TileMap>(level_tilemap_entity,
                                        std::move(level.tile_map));
    std::size_t current_area_index = 0;
    std::size_t current_tilemap_index = 0;
    std::optional<DoubleTimeGoalArea> current_goal = level.goal;

    std::vector<ServerPlatform> platforms;
    for (const DoubleTimeMovingPlatform &definition : level.moving_platforms) {
        const auto to_transform = [&](std::int32_t x, std::int32_t y) {
            return svanes::Transform{
                tile_map_transform.x +
                    (x + definition.width * 0.5F) * kDoubleTimeTileSize,
                tile_map_transform.y +
                    (y + definition.height * 0.5F) * kDoubleTimeTileSize,
                0.0F};
        };
        const svanes::Transform start =
            to_transform(definition.start_x, definition.start_y);
        const svanes::Transform end =
            to_transform(definition.end_x, definition.end_y);
        const svanes::Entity entity = active_world->CreateEntity();
        active_world->AddComponent<svanes::Transform>(entity, start);
        active_world->AddComponent<svanes::Collider2D>(
            entity, svanes::Collider2D{svanes::Rectangle2D{
                         0.0F, 0.0F,
                         definition.width * kDoubleTimeTileSize,
                         definition.height * kDoubleTimeTileSize}});
        active_world->AddComponent<RigidBody>(entity, RigidBody{});
        platforms.push_back(
            ServerPlatform{definition, entity, start, end, start});
    }
    std::vector<ServerDoor> doors;
    for (const DoubleTimeDoor &definition : level.doors) {
        const svanes::Transform transform{
            tile_map_transform.x +
                (definition.x + definition.width * 0.5F) * kDoubleTimeTileSize,
            tile_map_transform.y +
                (definition.y + definition.height * 0.5F) * kDoubleTimeTileSize,
            0.0F};
        const svanes::Entity entity = active_world->CreateEntity();
        active_world->AddComponent<svanes::Transform>(entity, transform);
        active_world->AddComponent<svanes::Collider2D>(
            entity, svanes::Collider2D{svanes::Rectangle2D{
                         0.0F, 0.0F,
                         definition.width * kDoubleTimeTileSize,
                         definition.height * kDoubleTimeTileSize}});
        active_world->AddComponent<RigidBody>(entity, RigidBody{});
        doors.push_back(ServerDoor{definition, entity, transform, transform});
    }
    std::vector<ServerAreaObject> death_zones;
    std::vector<ServerAreaObject> spikes;
    std::vector<ServerAreaObject> collectibles;
    const auto create_area_objects = [&](const DoubleTimeLevelData &data) {
        const auto create = [&](const DoubleTimeLevelArea &definition,
                                bool networked) {
            const svanes::Transform transform{
                tile_map_transform.x +
                    (definition.x + definition.width * 0.5F) * kDoubleTimeTileSize,
                tile_map_transform.y +
                    (definition.y + definition.height * 0.5F) * kDoubleTimeTileSize,
                0.0F};
            const svanes::Entity entity = active_world->CreateEntity();
            active_world->AddComponent<svanes::Transform>(entity, transform);
            if (networked) {
            }
            return ServerAreaObject{definition, entity};
        };
        for (const auto &definition : data.death_zones)
            death_zones.push_back(create(definition, false));
        for (const auto &definition : data.spike_zones)
            spikes.push_back(create(definition, true));
        for (const auto &definition : data.collectibles)
            collectibles.push_back(create(definition, true));
    };
    create_area_objects(level);

    const auto rebuild_interactables = [&](const DoubleTimeLevelData &next_level) {
        for (const ServerPlatform &platform : platforms) {
            active_world->DestroyEntity(platform.entity);
        }
        for (const ServerDoor &door : doors) {
            active_world->DestroyEntity(door.entity);
        }
        platforms.clear();
        doors.clear();
        for (const auto &object : death_zones) active_world->DestroyEntity(object.entity);
        for (const auto &object : spikes) active_world->DestroyEntity(object.entity);
        for (const auto &object : collectibles) active_world->DestroyEntity(object.entity);
        death_zones.clear(); spikes.clear(); collectibles.clear();
        for (const DoubleTimeMovingPlatform &definition : next_level.moving_platforms) {
            const auto to_transform = [&](std::int32_t x, std::int32_t y) {
                return svanes::Transform{
                    tile_map_transform.x +
                        (x + definition.width * 0.5F) * kDoubleTimeTileSize,
                    tile_map_transform.y +
                        (y + definition.height * 0.5F) * kDoubleTimeTileSize,
                    0.0F};
            };
            const svanes::Transform start =
                to_transform(definition.start_x, definition.start_y);
            const svanes::Transform end =
                to_transform(definition.end_x, definition.end_y);
            const svanes::Entity entity = active_world->CreateEntity();
            active_world->AddComponent<svanes::Transform>(entity, start);
            active_world->AddComponent<svanes::Collider2D>(
                entity, svanes::Collider2D{svanes::Rectangle2D{
                             0.0F, 0.0F,
                             definition.width * kDoubleTimeTileSize,
                             definition.height * kDoubleTimeTileSize}});
            active_world->AddComponent<RigidBody>(entity, RigidBody{});
            platforms.push_back(ServerPlatform{definition, entity, start, end, start});
        }
        for (const DoubleTimeDoor &definition : next_level.doors) {
            const svanes::Transform transform{
                tile_map_transform.x +
                    (definition.x + definition.width * 0.5F) * kDoubleTimeTileSize,
                tile_map_transform.y +
                    (definition.y + definition.height * 0.5F) * kDoubleTimeTileSize,
                0.0F};
            const svanes::Entity entity = active_world->CreateEntity();
            active_world->AddComponent<svanes::Transform>(entity, transform);
            active_world->AddComponent<svanes::Collider2D>(
                entity, svanes::Collider2D{svanes::Rectangle2D{
                             0.0F, 0.0F,
                             definition.width * kDoubleTimeTileSize,
                             definition.height * kDoubleTimeTileSize}});
            active_world->AddComponent<RigidBody>(entity, RigidBody{});
            doors.push_back(ServerDoor{definition, entity, transform, transform});
        }
        create_area_objects(next_level);
    };

    std::optional<ServerCharacter> character;
    std::optional<svanes::ClientId> platform_client_id;
    bool platform_left_held = false;
    bool platform_right_held = false;
    bool platform_up_held = false;
    bool platform_down_held = false;
    bool input_left_on_beat = false;
    bool input_right_on_beat = false;
    bool input_up_on_beat = false;
    bool input_down_on_beat = false;
    bool input_action_on_beat = false;
    bool platform_left_was_held = false;
    bool platform_right_was_held = false;
    bool platform_up_was_held = false;
    bool platform_down_was_held = false;
    PlatformMotionTrack left_motion;
    PlatformMotionTrack right_motion;
    PlatformMotionTrack up_motion;
    PlatformMotionTrack down_motion;
    std::optional<std::size_t> character_platform_index;
    const svanes::Vector2D gravity{
        0.0F,
        svanes::PerSecondSquaredToPerTicSquared(kGravityPerSecondSquared)};

    auto broadcast = [&](const DoubleTimeEntityState &state) {
        network_server.Broadcast(svanes::NetworkMessage::From(state));
    };
    DoubleTimeServerRuntime runtime(
        [&](const svanes::NetworkMessage &message) {
            const PlayerInputMessage input = message.As<PlayerInputMessage>();
            const svanes::ClientId assigned_client_id = input.client_id;

            if (!character.has_value() && !input.platform_role) {
                const svanes::Entity entity = SpawnCharacter(
                    *active_world, character_spawn_x, character_spawn_y);
                character = ServerCharacter{entity, assigned_client_id};
                SDL_Log("Server: spawned the character for client %u.",
                        assigned_client_id);
            }

            if (!input.platform_role && character->client_id == assigned_client_id) {
                character->horizontal_input = input.horizontal;
                character->jump_requested =
                    character->jump_requested || input.action_requested;
                character->jump_on_beat =
                    character->jump_on_beat || input.jump_on_beat;
                return;
            }
            if (input.platform_role && !platform_client_id.has_value()) {
                platform_client_id = assigned_client_id;
                SDL_Log("Server: client %u controls moving platforms.",
                        assigned_client_id);
            }
            if (input.platform_role && platform_client_id == assigned_client_id) {
                platform_left_held = input.platform_left;
                platform_right_held = input.platform_right;
                platform_up_held = input.platform_up;
                platform_down_held = input.platform_down;
                input_left_on_beat = input.platform_left_on_beat;
                input_right_on_beat = input.platform_right_on_beat;
                input_up_on_beat = input.platform_up_on_beat;
                input_down_on_beat = input.platform_down_on_beat;
                input_action_on_beat = input.platform_action_on_beat;
                const svanes::TicCount server_tics =
                    active_world->GetComponent<svanes::Timeline>(
                        authoritative_timeline_entity)
                        .GetTotalTics();
                if (input.platform_action_requested) {
                    for (ServerDoor &door : doors) {
                        if (input_action_on_beat) {
                            door.open = true;
                            door.close_tics = server_tics +
                                static_cast<svanes::TicCount>(
                                    std::llround(
                                        double_time::BeatDurationTics(
                                            DoubleTimeAssetCatalog()
                                                [current_area_index]
                                                .beats_per_minute) *
                                        kPlatformHoldBeats));
                            door.nudge_remaining_seconds = 0.0F;
                            door.transform = door.base_transform;
                            active_world->RemoveComponent<svanes::Collider2D>(
                                door.entity);
                            active_world->RemoveComponent<RigidBody>(door.entity);
                        } else if (!door.open) {
                            door.nudge_remaining_seconds =
                                kPlatformTravelDurationSeconds;
                            door.transform = door.base_transform;
                            door.transform.x += kDoubleTimeTileSize * 0.15F;
                        }
                    }
                }
            }
        },
        [&]() {
            if (character.has_value()) {
                svanes::Kinematic2D &motion =
                    active_world->GetComponent<svanes::Kinematic2D>(character->entity);
                const float target_velocity =
                    character->horizontal_input *
                    svanes::PerSecondToPerTic(kCharacterMaxSpeedPerSecond);
                const float acceleration =
                    character->horizontal_input == 0.0F
                        ? kCharacterDecelerationPerSecondSquared
                        : kCharacterAccelerationPerSecondSquared;
                const float max_velocity_change =
                    svanes::PerSecondSquaredToPerTicSquared(acceleration) *
                    static_cast<float>(svanes::DefaultPhysicsStepTics);
                motion.velocity_x = std::clamp(
                    target_velocity, motion.velocity_x - max_velocity_change,
                    motion.velocity_x + max_velocity_change);
            }
            const float step_seconds = static_cast<float>(
                svanes::DefaultPhysicsStepTics) /
                static_cast<float>(svanes::TicsPerSecond);
            const svanes::TicCount server_tics =
                active_world->GetComponent<svanes::Timeline>(
                    authoritative_timeline_entity)
                    .GetTotalTics();
            for (ServerDoor &door : doors) {
                if (door.nudge_remaining_seconds > 0.0F) {
                    door.nudge_remaining_seconds -= step_seconds;
                    if (door.nudge_remaining_seconds <= 0.0F) {
                        door.nudge_remaining_seconds = 0.0F;
                        door.transform = door.base_transform;
                    }
                }
                if (door.open && door.close_tics.has_value() &&
                    server_tics >= *door.close_tics) {
                    door.open = false;
                    door.close_tics.reset();
                    door.transform = door.base_transform;
                    active_world->AddComponent<svanes::Collider2D>(
                        door.entity, svanes::Collider2D{svanes::Rectangle2D{
                            0.0F, 0.0F,
                            door.definition.width * kDoubleTimeTileSize,
                            door.definition.height * kDoubleTimeTileSize}});
                    active_world->AddComponent<RigidBody>(door.entity, RigidBody{});
                }
            }
            const auto advance_motion = [&](PlatformMotionTrack &track,
                                             bool vertical) {
                if (track.phase != PlatformMotionPhase::Idle) {
                    if (track.phase == PlatformMotionPhase::Holding) {
                        if (track.return_tics.has_value() &&
                            server_tics >= *track.return_tics) {
                            track.elapsed = 0.0F;
                            track.phase = PlatformMotionPhase::MovingToStart;
                        }
                        return;
                    }
                    track.elapsed += step_seconds;
                    const float progress = std::clamp(
                        track.elapsed / kPlatformTravelDurationSeconds,
                        0.0F, 1.0F);
                    const bool moving_to_end =
                        track.phase == PlatformMotionPhase::MovingToEnd;
                    for (ServerPlatform &platform : platforms) {
                        const float movement_delta = vertical
                            ? platform.end_transform.y - platform.start_transform.y
                            : platform.end_transform.x - platform.start_transform.x;
                        if (track.direction * movement_delta <= 0.0F) {
                            continue;
                        }
                        const svanes::Transform &from = moving_to_end
                            ? platform.start_transform
                            : (track.nudge ? svanes::Transform{
                                  platform.start_transform.x +
                                      (platform.end_transform.x -
                                       platform.start_transform.x) * 0.15F,
                                  platform.start_transform.y +
                                      (platform.end_transform.y -
                                       platform.start_transform.y) * 0.15F,
                                  0.0F}
                                           : platform.end_transform);
                        const svanes::Transform &to = moving_to_end
                            ? (track.nudge ? svanes::Transform{
                                  platform.start_transform.x +
                                      (platform.end_transform.x -
                                       platform.start_transform.x) * 0.15F,
                                  platform.start_transform.y +
                                      (platform.end_transform.y -
                                       platform.start_transform.y) * 0.15F,
                                  0.0F}
                                           : platform.end_transform)
                            : platform.start_transform;
                        active_world->GetComponent<svanes::Transform>(platform.entity) =
                            svanes::Transform{
                                from.x + (to.x - from.x) * progress,
                                from.y + (to.y - from.y) * progress, 0.0F};
                    }
                    if (progress >= 1.0F) {
                        track.elapsed = 0.0F;
                        if (moving_to_end && !track.nudge) {
                            track.phase = PlatformMotionPhase::Holding;
                        } else if (moving_to_end && track.nudge) {
                            track.phase = PlatformMotionPhase::MovingToStart;
                        } else {
                            track.phase = PlatformMotionPhase::Idle;
                            track.return_tics.reset();
                            track.nudge = false;
                        }
                    }
                }
            };
            advance_motion(left_motion, false);
            advance_motion(right_motion, false);
            advance_motion(up_motion, true);
            advance_motion(down_motion, true);
            const bool left_pressed = platform_left_held && !platform_left_was_held;
            const bool right_pressed = platform_right_held && !platform_right_was_held;
            const bool up_pressed = platform_up_held && !platform_up_was_held;
            const bool down_pressed = platform_down_held && !platform_down_was_held;
            const auto start_motion = [&](PlatformMotionTrack &track,
                                          bool pressed, bool on_beat,
                                          float direction) {
                if (!pressed || track.phase != PlatformMotionPhase::Idle) {
                    return;
                }
                track.direction = direction;
                track.elapsed = 0.0F;
                track.nudge = !on_beat;
                track.return_tics = on_beat
                    ? std::optional<svanes::TicCount>{
                          server_tics + static_cast<svanes::TicCount>(
                                            std::llround(
                                                double_time::BeatDurationTics(
                                                    DoubleTimeAssetCatalog()
                                                        [current_area_index]
                                                        .beats_per_minute) *
                                                kPlatformHoldBeats))}
                    : std::nullopt;
                track.phase = PlatformMotionPhase::MovingToEnd;
            };
            start_motion(left_motion, left_pressed, input_left_on_beat, -1.0F);
            start_motion(right_motion, right_pressed, input_right_on_beat, 1.0F);
            start_motion(up_motion, up_pressed, input_up_on_beat, -1.0F);
            start_motion(down_motion, down_pressed, input_down_on_beat, 1.0F);
            if (character.has_value()) {
                svanes::Transform &character_transform =
                    active_world->GetComponent<svanes::Transform>(character->entity);
                svanes::Kinematic2D &character_motion =
                    active_world->GetComponent<svanes::Kinematic2D>(character->entity);
                if (character_platform_index.has_value() &&
                    character_motion.velocity_y == 0.0F) {
                    ServerPlatform &platform =
                        platforms[*character_platform_index];
                    const svanes::Transform current_transform =
                        active_world->GetComponent<svanes::Transform>(platform.entity);
                    character_transform.x +=
                        current_transform.x - platform.previous_transform.x;
                    character_transform.y +=
                        current_transform.y - platform.previous_transform.y;
                }
            }
            for (ServerPlatform &platform : platforms) {
                platform.previous_transform =
                    active_world->GetComponent<svanes::Transform>(platform.entity);
            }
            platform_left_was_held = platform_left_held;
            platform_right_was_held = platform_right_held;
            platform_up_was_held = platform_up_held;
            platform_down_was_held = platform_down_held;
        },
        [&](std::span<const svanes::PhysicsTimeStep>) {
                    if (character.has_value()) {
                        svanes::Transform &transform =
                            active_world->GetComponent<svanes::Transform>(
                                character->entity);
                        ResolveCharacterAxis(*active_world, character->entity, true);
                        ResolveCharacterAxis(*active_world, character->entity, false);
                        svanes::Kinematic2D &motion =
                            active_world->GetComponent<svanes::Kinematic2D>(
                                character->entity);
                        const auto overlaps = [&](const ServerAreaObject &object) {
                            const auto &object_transform =
                                active_world->GetComponent<svanes::Transform>(object.entity);
                            const float half_width = object.definition.width *
                                                     kDoubleTimeTileSize * 0.5F;
                            const float half_height = object.definition.height *
                                                      kDoubleTimeTileSize * 0.5F;
                            return transform.x + kDoubleTimeCharacterWidth * 0.5F >
                                       object_transform.x - half_width &&
                                   transform.x - kDoubleTimeCharacterWidth * 0.5F <
                                       object_transform.x + half_width &&
                                   transform.y + kDoubleTimeCharacterHeight * 0.5F >
                                       object_transform.y - half_height &&
                                   transform.y - kDoubleTimeCharacterHeight * 0.5F <
                                       object_transform.y + half_height;
                        };
                        bool should_respawn = false;
                        for (const auto &zone : death_zones) {
                            if (overlaps(zone)) {
                                should_respawn = true;
                                break;
                            }
                        }
                        if (!should_respawn) {
                            for (const auto &zone : spikes) {
                                if (zone.enabled && overlaps(zone)) {
                                    should_respawn = true;
                                    break;
                                }
                            }
                        }
                        for (auto &collectible : collectibles) {
                            if (collectible.enabled && overlaps(collectible)) {
                                collectible.enabled = false;
                                active_world->RemoveComponent<svanes::Collider2D>(
                                    collectible.entity);
                            }
                        }
                        if (should_respawn) {
                            transform = svanes::Transform{
                                character_spawn_x, character_spawn_y, 0.0F};
                            motion.velocity_x = 0.0F;
                            motion.velocity_y = 0.0F;
                        }

                        if (current_goal.has_value() &&
                            current_area_index < DoubleTimeAssetCatalog().size()) {
                            const auto &goal = *current_goal;
                            const float goal_left =
                                tile_map_transform.x +
                                goal.x * kDoubleTimeTileSize;
                            const float goal_right =
                                goal_left + goal.width * kDoubleTimeTileSize;
                            const float goal_top =
                                tile_map_transform.y +
                                goal.y * kDoubleTimeTileSize;
                            const float goal_bottom =
                                goal_top + goal.height * kDoubleTimeTileSize;
                            const float character_left =
                                transform.x - kDoubleTimeCharacterWidth * 0.5F;
                            const float character_right =
                                transform.x + kDoubleTimeCharacterWidth * 0.5F;
                            const float character_top =
                                transform.y - kDoubleTimeCharacterHeight * 0.5F;
                            const float character_bottom =
                                transform.y + kDoubleTimeCharacterHeight * 0.5F;
                            const auto &area = DoubleTimeAssetCatalog()[current_area_index];
                            const bool reached_goal =
                                character_right > goal_left &&
                                character_left < goal_right &&
                                character_bottom > goal_top &&
                                character_top < goal_bottom;
                            const bool has_next_tilemap =
                                current_tilemap_index + 1 < area.tilemap_filenames.size();
                            const auto next_area = area.next_area;
                            if (reached_goal && (has_next_tilemap || next_area)) {
                                const std::size_t target_area = has_next_tilemap
                                    ? current_area_index : *next_area;
                                const std::size_t target_tilemap = has_next_tilemap
                                    ? current_tilemap_index + 1 : 0;
                                DoubleTimeLevelData next_level =
                                    LoadDoubleTimeLevelData(
                                        std::string{DOUBLE_TIME_GAME_ASSETS_DIR} +
                                        "/" + std::string{DoubleTimeAssetCatalog()[target_area]
                                                               .tilemap_filenames[target_tilemap]});
                                active_world->DestroyEntity(level_tilemap_entity);
                                level_tilemap_entity = active_world->CreateEntity();
                                tile_map_transform = GetCenteredTileMapTransform(
                                    next_level.tile_map,
                                    kDoubleTimeWorldWidth * 0.5F,
                                    kDoubleTimeWorldHeight * 0.5F);
                                active_world->AddComponent<svanes::Transform>(
                                    level_tilemap_entity, tile_map_transform);
                                active_world->AddComponent<svanes::TileMap>(
                                    level_tilemap_entity,
                                    std::move(next_level.tile_map));
                                rebuild_interactables(next_level);
                                current_area_index = target_area;
                                current_tilemap_index = target_tilemap;
                                current_goal = next_level.goal;
                                character_spawn_x = next_level.spawn_point
                                    ? tile_map_transform.x +
                                          (next_level.spawn_point->x + 0.5F) *
                                              kDoubleTimeTileSize
                                    : tile_map_transform.x + kCharacterSpawnLeft;
                                character_spawn_y = next_level.spawn_point
                                    ? tile_map_transform.y +
                                          (next_level.spawn_point->y + 0.5F) *
                                              kDoubleTimeTileSize
                                    : tile_map_transform.y + kCharacterSpawnTop;
                                transform = svanes::Transform{
                                    character_spawn_x, character_spawn_y, 0.0F};
                                motion.velocity_x = 0.0F;
                                motion.velocity_y = 0.0F;
                            }
                        }

                        const bool is_grounded = motion.velocity_y == 0.0F;
                        const float step_seconds = static_cast<float>(
                            svanes::DefaultPhysicsStepTics) /
                            static_cast<float>(svanes::TicsPerSecond);
                        if (is_grounded) {
                            character->time_since_grounded = 0.0F;
                        } else {
                            character->time_since_grounded += step_seconds;
                        }
                        if (character->jump_requested &&
                            character->time_since_grounded <=
                                kCoyoteTimeSeconds) {
                            const float jump_speed =
                                kJumpSpeedPerSecond *
                                (character->jump_on_beat
                                     ? 1.0F
                                     : kOffBeatJumpForceMultiplier);
                            motion.velocity_y =
                                -svanes::PerSecondToPerTic(jump_speed);
                        }
                        character_platform_index.reset();
                        if (motion.velocity_y == 0.0F) {
                            const float character_left =
                                transform.x - kCharacterColliderWidth * 0.5F;
                            const float character_right =
                                transform.x + kCharacterColliderWidth * 0.5F;
                            const float character_bottom =
                                transform.y + kDoubleTimeCharacterHeight * 0.5F;
                            for (std::size_t index = 0; index < platforms.size();
                                 ++index) {
                                const ServerPlatform &platform = platforms[index];
                                const svanes::Transform platform_transform =
                                    active_world->GetComponent<svanes::Transform>(
                                        platform.entity);
                                const float platform_left =
                                    platform_transform.x -
                                    platform.definition.width *
                                        kDoubleTimeTileSize * 0.5F;
                                const float platform_right =
                                    platform_transform.x +
                                    platform.definition.width *
                                        kDoubleTimeTileSize * 0.5F;
                                const float platform_top =
                                    platform_transform.y -
                                    platform.definition.height *
                                        kDoubleTimeTileSize * 0.5F;
                                if (character_right > platform_left &&
                                    character_left < platform_right &&
                                    std::abs(character_bottom - platform_top) <
                                        0.2F) {
                                    character_platform_index = index;
                                    break;
                                }
                            }
                        }
                        character->jump_requested = false;
                        character->jump_on_beat = false;
                    }
        },
        [&]() {
            if (character.has_value()) {
                const svanes::Transform transform =
                    active_world->GetComponent<svanes::Transform>(character->entity);
                broadcast(DoubleTimeEntityState{
                    transform, character->horizontal_input != 0.0F, 0,
                    DoubleTimeEntityKind::Character, kDoubleTimeCharacterWidth,
                    kDoubleTimeCharacterHeight});
            }
            for (const ServerPlatform &platform : platforms) {
                const svanes::Transform transform =
                    active_world->GetComponent<svanes::Transform>(platform.entity);
                broadcast(DoubleTimeEntityState{
                    transform, false, platform.definition.id,
                    DoubleTimeEntityKind::MovingPlatform,
                    platform.definition.width * kDoubleTimeTileSize,
                    platform.definition.height * kDoubleTimeTileSize});
            }
            for (const ServerDoor &door : doors) {
                broadcast(DoubleTimeEntityState{
                    door.transform, false, door.definition.id,
                    DoubleTimeEntityKind::Door,
                    door.definition.width * kDoubleTimeTileSize,
                    door.definition.height * kDoubleTimeTileSize, 0,
                    !door.open});
            }
            for (const ServerAreaObject &spike : spikes) {
                const auto transform = active_world->GetComponent<svanes::Transform>(spike.entity);
                broadcast(DoubleTimeEntityState{
                    transform, false, static_cast<std::uint32_t>(spike.entity), DoubleTimeEntityKind::Spikes,
                    spike.definition.width * kDoubleTimeTileSize,
                    spike.definition.height * kDoubleTimeTileSize, 0,
                    spike.enabled});
            }
            for (const ServerAreaObject &collectible : collectibles) {
                const auto transform = active_world->GetComponent<svanes::Transform>(collectible.entity);
                broadcast(DoubleTimeEntityState{
                    transform, false, static_cast<std::uint32_t>(collectible.entity), DoubleTimeEntityKind::Collectible,
                    collectible.definition.width * kDoubleTimeTileSize,
                    collectible.definition.height * kDoubleTimeTileSize, 0,
                    collectible.enabled});
            }
            broadcast(DoubleTimeEntityState{
                {}, false, 0, DoubleTimeEntityKind::AudioTimeline, 0.0F,
                0.0F,
                active_world->GetComponent<svanes::Timeline>(
                    authoritative_timeline_entity)
                    .GetTotalTics(),
                true,
                static_cast<std::uint32_t>(current_area_index),
                static_cast<std::uint32_t>(current_tilemap_index)});
        },
        gravity,
        [&](svanes::ServerContext &context) {
            context.network.Listen("*:" +
                                   std::to_string(kDoubleTimeServerJoinPort));
            context.world = std::move(world);
            active_world = &context.world;
        });
    return network_server.run(runtime);
}
