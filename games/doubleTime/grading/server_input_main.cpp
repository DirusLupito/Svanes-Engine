#include "asset_catalog.hpp"
#include "network_protocol.hpp"
#include "rhythm_helpers.hpp"
#include "tilemap_parser.hpp"

#include <svanes/async/async_parallel_for_driver.hpp>
#include <svanes/collision_system.hpp>
#include <svanes/kinematic_system.hpp>
#include <svanes/network/clientserver/network_server.hpp>
#include <svanes/network/clientserver/IServer.hpp>

#include <svanes/registry.hpp>
#include <svanes/timeline_system.hpp>

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
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
constexpr float kCharacterColliderWidth = kDoubleTimeCharacterWidth * 0.8F;
constexpr float kCharacterColliderOffsetX = 0.0F;
constexpr float kPlatformPeriodSeconds = 4.0F;
constexpr float kDisconnectTimeoutSeconds = 2.0F;
constexpr std::int32_t kMaxResolutionIterations = 4;

struct RigidBody {};

struct GradingCharacter {
    svanes::Entity entity;
    svanes::ClientId client_id;
    float horizontal_input = 0.0F;
    bool jump_requested = false;
    bool jump_on_beat = false;
    float time_since_grounded = 0.0F;
    std::optional<std::size_t> platform_index;
    svanes::TicCount last_input_tics = 0;
};

struct GradingPlatform {
    DoubleTimeMovingPlatform definition;
    svanes::Entity entity;
    svanes::Transform start;
    svanes::Transform end;
    svanes::Transform previous;
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
        const auto consider = [&](const svanes::Collision2D &collision) {
            const bool axis_aligned = horizontal ? collision.normal.y == 0.0F
                                                 : collision.normal.x == 0.0F;
            if (axis_aligned &&
                (!deepest_collision.has_value() ||
                 collision.penetration_depth >
                     deepest_collision->penetration_depth)) {
                deepest_collision = collision;
            }
        };
        world.ForEach<svanes::Transform, svanes::Collider2D, RigidBody>(
            [&](svanes::Entity candidate_entity,
                const svanes::Transform &candidate_transform,
                const svanes::Collider2D &candidate_collider,
                const RigidBody &) {
                if (candidate_entity == character_entity) return;
                for (const auto &collision : svanes::DetectCollisions(
                         character_collider.geometry, character_transform,
                         candidate_collider.geometry, candidate_transform)) {
                    consider(collision);
                }
            });
        world.ForEach<svanes::Transform, svanes::TileMap>(
            [&](svanes::Entity, const svanes::Transform &tilemap_transform,
                const svanes::TileMap &tilemap) {
                for (const auto &collision : svanes::DetectTileMapCollisions(
                         character_collider.geometry, character_transform,
                         tilemap, tilemap_transform)) {
                    consider(collision);
                }
            });
        if (!deepest_collision.has_value()) break;
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
    world.AddComponent<svanes::Transform>(entity, svanes::Transform{x, y, 0.0F});
    if constexpr (kDoubleTimeNetworkingStrategy ==
                  DoubleTimeNetworkingStrategy::InputAuthoritative) {
    world.AddComponent<svanes::Kinematic2D>(entity, svanes::Kinematic2D{});
    world.AddComponent<svanes::Gravity>(entity, svanes::Gravity{});
    world.AddComponent<svanes::Collider2D>(
        entity, svanes::Collider2D{svanes::Rectangle2D{
                    kCharacterColliderOffsetX, 0.0F, kCharacterColliderWidth,
                    kDoubleTimeCharacterHeight}});
    }
    world.AddComponent<svanes::Timeline>(entity);
        return entity;
}

}

class GradingServerRuntime final : public svanes::IServer {
public:
    using InitializeCallback = std::function<void(svanes::ServerContext &)>;
    using Callback = std::function<void(const svanes::NetworkMessage &)>;
    using UpdateCallback = std::function<void()>;
    using PhysicsCallback = std::function<void(std::span<const svanes::PhysicsTimeStep>)>;
    GradingServerRuntime(Callback input, UpdateCallback update,
                         PhysicsCallback physics, UpdateCallback post,
                         svanes::Vector2D gravity,
                         InitializeCallback initialize_callback)
        : input(std::move(input)), update(std::move(update)),
          physics(std::move(physics)), post(std::move(post)), gravity(gravity),
          initialize_callback(std::move(initialize_callback)) {}
    void Initialize(svanes::ServerContext &context) override {
        initialize_callback(context);
        context.gravity = gravity;
    }
    svanes::JoinResponse HandleJoinRequest(const svanes::ServerJoinContext &) override { return {true, {}}; }
    void Update(const svanes::ServerUpdateContext &context) override {
        for (const auto &received : context.network.PollInputs()) {
            if constexpr (kDoubleTimeNetworkingStrategy == DoubleTimeNetworkingStrategy::ClientPositionAuthoritative) {
                auto message = received.message.As<PlayerTransformMessage>();
                message.client_id = received.client;
                input(svanes::NetworkMessage::From(message));
            } else {
                auto message = received.message.As<PlayerInputMessage>();
                message.client_id = received.client;
                input(svanes::NetworkMessage::From(message));
            }
        }
        update();
    }
    void PhysicsUpdate(const svanes::ServerPhysicsContext &context) override {
        physics(context.entity_steps);
        post();
    }
private:
    Callback input;
    UpdateCallback update;
    PhysicsCallback physics;
    UpdateCallback post;
    svanes::Vector2D gravity;
    InitializeCallback initialize_callback;
};

int32_t main(int32_t argc, char **argv) {
    if (argc > 1) {
        throw std::invalid_argument(
            "The Double Time grading server takes no arguments.");
    }
    svanes::NetworkServer network_server;
    svanes::Registry world;
    svanes::Registry *active_world = &world;
    const svanes::Entity timeline_entity = world.CreateEntity();
    world.AddComponent<svanes::Timeline>(timeline_entity);

    const auto &area = DoubleTimeAssetCatalog().front();
    DoubleTimeLevelData level = LoadDoubleTimeLevelData(
        std::string{DOUBLE_TIME_GAME_ASSETS_DIR} + "/" +
        std::string{area.tilemap_filenames.front()});
    const svanes::Entity tilemap_entity = world.CreateEntity();
    const svanes::Transform tilemap_transform = GetCenteredTileMapTransform(
        level.tile_map, kDoubleTimeWorldWidth * 0.5F,
        kDoubleTimeWorldHeight * 0.5F);
    world.AddComponent<svanes::Transform>(tilemap_entity, tilemap_transform);
    world.AddComponent<svanes::TileMap>(tilemap_entity, std::move(level.tile_map));

    const float spawn_x = level.spawn_point
        ? tilemap_transform.x + (level.spawn_point->x + 0.5F) * kDoubleTimeTileSize
        : 20.0F;
    const float spawn_y = level.spawn_point
        ? tilemap_transform.y + (level.spawn_point->y + 0.5F) * kDoubleTimeTileSize
        : 20.0F;

    std::vector<GradingPlatform> platforms;
    for (const auto &definition : level.moving_platforms) {
        const auto make_transform = [&](std::int32_t x, std::int32_t y) {
            return svanes::Transform{
                tilemap_transform.x +
                    (x + definition.width * 0.5F) * kDoubleTimeTileSize,
                tilemap_transform.y +
                    (y + definition.height * 0.5F) * kDoubleTimeTileSize,
                0.0F};
        };
        const svanes::Transform start =
            make_transform(definition.start_x, definition.start_y);
        const svanes::Transform end =
            make_transform(definition.end_x, definition.end_y);
        const svanes::Entity entity = world.CreateEntity();
        world.AddComponent<svanes::Transform>(entity, start);
        world.AddComponent<svanes::Collider2D>(
            entity, svanes::Collider2D{svanes::Rectangle2D{
                         0.0F, 0.0F, definition.width * kDoubleTimeTileSize,
                         definition.height * kDoubleTimeTileSize}});
        world.AddComponent<RigidBody>(entity, RigidBody{});
                platforms.push_back(GradingPlatform{definition, entity, start, end, start});
    }

    std::map<svanes::ClientId, GradingCharacter> characters;
    const svanes::Vector2D gravity{
        0.0F,
        svanes::PerSecondSquaredToPerTicSquared(kGravityPerSecondSquared)};
    auto broadcast = [&](const DoubleTimeEntityState &state) {
        network_server.Broadcast(svanes::NetworkMessage::From(state));
    };
    GradingServerRuntime runtime(
        [&](const svanes::NetworkMessage &message) {
            if constexpr (kDoubleTimeNetworkingStrategy ==
                          DoubleTimeNetworkingStrategy::ClientPositionAuthoritative) {
                const PlayerTransformMessage update =
                    message.As<PlayerTransformMessage>();
                const svanes::TicCount now = active_world->GetComponent<svanes::Timeline>(
                    timeline_entity).GetTotalTics();
                auto found = characters.find(update.client_id);
                if (found == characters.end()) {
                    const svanes::Entity entity =
                        SpawnCharacter(*active_world, update.transform.x,
                                       update.transform.y);
                    found = characters.emplace(
                        update.client_id,
                        GradingCharacter{entity, update.client_id}).first;
                }
                active_world->GetComponent<svanes::Transform>(found->second.entity) =
                    update.transform;
                found->second.last_input_tics = now;
                return;
            }
            const PlayerInputMessage input = message.As<PlayerInputMessage>();
            if constexpr (kDoubleTimeNetworkingStrategy ==
                          DoubleTimeNetworkingStrategy::ClientPositionAuthoritative) {
                return;
            }
            const svanes::TicCount now = active_world->GetComponent<svanes::Timeline>(
                timeline_entity).GetTotalTics();
            auto found = characters.find(input.client_id);
            if (found == characters.end()) {
                const svanes::Entity entity = SpawnCharacter(*active_world, spawn_x, spawn_y);
                found = characters.emplace(
                    input.client_id,
                    GradingCharacter{entity, input.client_id}).first;
                SDL_Log("Grading server: spawned character for client %u.",
                        input.client_id);
            }
            GradingCharacter &character = found->second;
            character.horizontal_input = input.horizontal;
            character.jump_requested =
                character.jump_requested || input.action_requested;
            character.jump_on_beat =
                character.jump_on_beat || input.jump_on_beat;
            character.last_input_tics = now;
        },
        [&]() {
            const auto timeline = active_world->GetComponent<svanes::Timeline>(
                timeline_entity).GetTotalTics();
            const float seconds = static_cast<float>(timeline) /
                                  static_cast<float>(svanes::TicsPerSecond);
            const float phase = std::fmod(seconds, kPlatformPeriodSeconds) /
                                kPlatformPeriodSeconds;
            const float wave = 0.5F - 0.5F * std::cos(phase * 2.0F * 3.14159265F);
            for (GradingPlatform &platform : platforms) {
                platform.previous = active_world->GetComponent<svanes::Transform>(
                    platform.entity);
                active_world->GetComponent<svanes::Transform>(platform.entity) =
                    svanes::Transform{
                        platform.start.x +
                            (platform.end.x - platform.start.x) * wave,
                        platform.start.y +
                            (platform.end.y - platform.start.y) * wave,
                        0.0F};
            }
            if constexpr (kDoubleTimeNetworkingStrategy ==
                          DoubleTimeNetworkingStrategy::ClientPositionAuthoritative) {
                return;
            }
            for (auto &[client_id, character] : characters) {
                (void)client_id;
                if (!character.platform_index.has_value()) continue;
                svanes::Kinematic2D &motion =
                    active_world->GetComponent<svanes::Kinematic2D>(character.entity);
                if (motion.velocity_y != 0.0F) continue;
                GradingPlatform &platform =
                    platforms[*character.platform_index];
                const svanes::Transform current =
                    active_world->GetComponent<svanes::Transform>(platform.entity);
                svanes::Transform &transform =
                    active_world->GetComponent<svanes::Transform>(character.entity);
                transform.x += current.x - platform.previous.x;
                transform.y += current.y - platform.previous.y;
            }
            for (auto &[client_id, character] : characters) {
                (void)client_id;
                svanes::Kinematic2D &motion =
                    active_world->GetComponent<svanes::Kinematic2D>(character.entity);
                const float target = character.horizontal_input *
                    svanes::PerSecondToPerTic(kCharacterMaxSpeedPerSecond);
                const float acceleration = character.horizontal_input == 0.0F
                    ? kCharacterDecelerationPerSecondSquared
                    : kCharacterAccelerationPerSecondSquared;
                const float change = svanes::PerSecondSquaredToPerTicSquared(
                    acceleration) * static_cast<float>(svanes::DefaultPhysicsStepTics);
                motion.velocity_x = std::clamp(
                    target, motion.velocity_x - change, motion.velocity_x + change);
                const bool grounded = motion.velocity_y == 0.0F;
                const float step_seconds = static_cast<float>(
                    svanes::DefaultPhysicsStepTics) /
                    static_cast<float>(svanes::TicsPerSecond);
                character.time_since_grounded = grounded
                    ? 0.0F : character.time_since_grounded + step_seconds;
                if (character.jump_requested &&
                    character.time_since_grounded <= kCoyoteTimeSeconds) {
                    const float jump_speed = kJumpSpeedPerSecond;
                    motion.velocity_y = -svanes::PerSecondToPerTic(jump_speed);
                }
                character.jump_requested = false;
                character.jump_on_beat = false;
            }
        },
        [&](std::span<const svanes::PhysicsTimeStep>) {
            if constexpr (kDoubleTimeNetworkingStrategy ==
                          DoubleTimeNetworkingStrategy::ClientPositionAuthoritative) {
                return;
            }
            for (auto &[client_id, character] : characters) {
                (void)client_id;
                ResolveCharacterAxis(*active_world, character.entity, true);
                ResolveCharacterAxis(*active_world, character.entity, false);
                character.platform_index.reset();
                const svanes::Kinematic2D &motion =
                    active_world->GetComponent<svanes::Kinematic2D>(character.entity);
                if (motion.velocity_y == 0.0F) {
                    const svanes::Transform transform =
                        active_world->GetComponent<svanes::Transform>(character.entity);
                    const float character_left =
                        transform.x - kCharacterColliderWidth * 0.5F;
                    const float character_right =
                        transform.x + kCharacterColliderWidth * 0.5F;
                    const float character_bottom =
                        transform.y + kDoubleTimeCharacterHeight * 0.5F;
                    for (std::size_t index = 0; index < platforms.size();
                         ++index) {
                        const GradingPlatform &platform = platforms[index];
                        const svanes::Transform platform_transform =
                            active_world->GetComponent<svanes::Transform>(
                                platform.entity);
                        const float platform_left =
                            platform_transform.x -
                            platform.definition.width * kDoubleTimeTileSize *
                                0.5F;
                        const float platform_right =
                            platform_transform.x +
                            platform.definition.width * kDoubleTimeTileSize *
                                0.5F;
                        const float platform_top =
                            platform_transform.y -
                            platform.definition.height * kDoubleTimeTileSize *
                                0.5F;
                        if (character_right > platform_left &&
                            character_left < platform_right &&
                            std::abs(character_bottom - platform_top) < 0.2F) {
                            character.platform_index = index;
                            break;
                        }
                    }
                }
                svanes::Transform &transform =
                    active_world->GetComponent<svanes::Transform>(character.entity);
                if (transform.y > kDoubleTimeWorldHeight * 2.0F) {
                    transform = svanes::Transform{spawn_x, spawn_y, 0.0F};
                    active_world->GetComponent<svanes::Kinematic2D>(character.entity)
                        .velocity_y = 0.0F;
                }
            }
        },
        [&]() {
            const svanes::TicCount now = active_world->GetComponent<svanes::Timeline>(
                timeline_entity).GetTotalTics();
            const svanes::TicCount timeout = static_cast<svanes::TicCount>(
                kDisconnectTimeoutSeconds * svanes::TicsPerSecond);
            for (auto it = characters.begin(); it != characters.end();) {
                if (now - it->second.last_input_tics > timeout) {
                    broadcast(DoubleTimeEntityState{
                        {}, false, it->first,
                        DoubleTimeEntityKind::CharacterDespawn, 0.0F, 0.0F});
                    active_world->DestroyEntity(it->second.entity);
                    it = characters.erase(it);
                } else {
                    ++it;
                }
            }
            for (const auto &[client_id, character] : characters) {
                const auto transform = active_world->GetComponent<svanes::Transform>(
                    character.entity);
                broadcast(DoubleTimeEntityState{
                    transform, character.horizontal_input != 0.0F, client_id,
                    DoubleTimeEntityKind::Character, kDoubleTimeCharacterWidth,
                    kDoubleTimeCharacterHeight});
            }
            for (const GradingPlatform &platform : platforms) {
                const auto transform = active_world->GetComponent<svanes::Transform>(
                    platform.entity);
                broadcast(DoubleTimeEntityState{
                    transform, false, platform.definition.id,
                    DoubleTimeEntityKind::MovingPlatform,
                    platform.definition.width * kDoubleTimeTileSize,
                    platform.definition.height * kDoubleTimeTileSize});
            }
            broadcast(DoubleTimeEntityState{
                {}, false, 0, DoubleTimeEntityKind::AudioTimeline, 0.0F, 0.0F,
                now, true, 0, 0});
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
