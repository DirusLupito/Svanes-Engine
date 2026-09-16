#include "network_protocol.hpp"

#include <svanes/async/async_parallel_for_driver.hpp>
#include <svanes/collision_system.hpp>
#include <svanes/kinematic_system.hpp>
#include <svanes/network/network_replication.hpp>
#include <svanes/network/network_server.hpp>
#include <svanes/physics_system.hpp>
#include <svanes/registry.hpp>
#include <svanes/timeline_system.hpp>
#include <svanes/vector2d.hpp>

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <thread>
#include <unordered_map>

namespace {

constexpr float kCharacterMoveSpeedPerSecond = 360.0F;
constexpr float kJumpSpeedPerSecond = 800.0F;
constexpr float kGravityPerSecondSquared = 1500.0F;

constexpr float kCharacterSpawnLeft = 64.0F;
constexpr float kCharacterSpawnTop = 64.0F;
constexpr float kCharacterSpawnSpacing = 220.0F;

constexpr std::int32_t kMaxResolutionIterations = 4;

struct ServerCharacter {
    svanes::Entity entity;
    float horizontal_input = 0.0F;
    bool jump_requested = false;
};

void ResolveCharacterAxis(svanes::Registry &world, svanes::Entity character_entity, bool horizontal) {
    svanes::Transform &character_transform = world.GetComponent<svanes::Transform>(character_entity);
    const svanes::Collider2D &character_collider = world.GetComponent<svanes::Collider2D>(character_entity);
    svanes::Kinematic2D &character_motion = world.GetComponent<svanes::Kinematic2D>(character_entity);

    for (std::int32_t iteration = 0; iteration < kMaxResolutionIterations; ++iteration) {
        std::optional<svanes::Collision2D> deepest_collision;

        world.ForEach<svanes::Transform, svanes::Collider2D>(
            [&](svanes::Entity candidate_entity, const svanes::Transform &candidate_transform,
                const svanes::Collider2D &candidate_collider) {
                if (candidate_entity == character_entity) {
                    return;
                }

                for (const svanes::Collision2D &collision : svanes::DetectCollisions(
                         character_collider.geometry, character_transform, candidate_collider.geometry,
                         candidate_transform
                     )) {
                    const bool is_axis_aligned = horizontal ? collision.normal.y == 0.0F : collision.normal.x == 0.0F;
                    if (!is_axis_aligned) {
                        continue;
                    }

                    if (!deepest_collision.has_value() || collision.penetration_depth > deepest_collision->penetration_depth) {
                        deepest_collision = collision;
                    }
                }
            }
        );

        if (!deepest_collision.has_value()) {
            break;
        }

        if (horizontal) {
            character_transform.x += deepest_collision->normal.x * deepest_collision->penetration_depth;
            character_motion.velocity_x = 0.0F;
        } else {
            character_transform.y += deepest_collision->normal.y * deepest_collision->penetration_depth;
            character_motion.velocity_y = 0.0F;
        }
    }
}

svanes::Entity SpawnCharacter(svanes::Registry &world, float x, float y) {
    const svanes::Entity entity = world.CreateEntity();
    world.AddComponent<svanes::Transform>(entity, svanes::Transform{x, y, 0.0F});
    world.AddComponent<svanes::Kinematic2D>(entity, svanes::Kinematic2D{});
    world.AddComponent<svanes::Gravity>(entity, svanes::Gravity{});
    world.AddComponent<svanes::Collider2D>(
        entity, svanes::Collider2D{svanes::Rectangle2D{0.0F, 0.0F, kChrisCharacterWidth, kChrisCharacterHeight}}
    );
    world.AddComponent<svanes::Timeline>(entity);
    world.AddComponent<svanes::Networked>(entity);
    return entity;
}

} // namespace

int32_t main() {
    svanes::NetworkServer network_server(kChrisStatePort, kChrisInputPort);

    svanes::Registry world;

    const float ground_top = kChrisWorldHeight - kChrisGroundHeight;
    const svanes::Entity ground_entity = world.CreateEntity();
    world.AddComponent<svanes::Transform>(
        ground_entity, svanes::Transform{kChrisWorldWidth * 0.5F, ground_top + kChrisGroundHeight * 0.5F, 0.0F}
    );
    world.AddComponent<svanes::Collider2D>(
        ground_entity, svanes::Collider2D{svanes::Rectangle2D{0.0F, 0.0F, kChrisWorldWidth, kChrisGroundHeight}}
    );

    const float platform_top = ground_top - kChrisPlatformTopHeightAboveGround;
    const svanes::Entity platform_entity = world.CreateEntity();
    world.AddComponent<svanes::Transform>(
        platform_entity,
        svanes::Transform{
            kChrisPlatformLeft + kChrisPlatformWidth * 0.5F, platform_top + kChrisPlatformHeight * 0.5F, 0.0F
        }
    );
    world.AddComponent<svanes::Collider2D>(
        platform_entity, svanes::Collider2D{svanes::Rectangle2D{0.0F, 0.0F, kChrisPlatformWidth, kChrisPlatformHeight}}
    );

    std::unordered_map<svanes::ClientId, ServerCharacter> characters;

    svanes::AsyncParallelForDriver driver(1);
    const svanes::Vector2D gravity{0.0F, svanes::PerSecondSquaredToPerTicSquared(kGravityPerSecondSquared)};

    auto previous_tick = std::chrono::steady_clock::now();
    svanes::TicCount pending_tics = 0;

    while (true) {
        for (const svanes::NetworkMessage &message : network_server.PollInbound()) {
            const PlayerInputMessage input = message.As<PlayerInputMessage>();

            auto existing = characters.find(input.client_id);
            if (existing == characters.end()) {
                const auto spawn_index = static_cast<float>(characters.size());
                const svanes::Entity entity = SpawnCharacter(
                    world, kCharacterSpawnLeft + spawn_index * kCharacterSpawnSpacing, kCharacterSpawnTop
                );
                existing = characters.emplace(input.client_id, ServerCharacter{entity}).first;
                SDL_Log("Server: spawned character for client %u.", input.client_id);
            }

            existing->second.horizontal_input = input.horizontal;
            existing->second.jump_requested = existing->second.jump_requested || input.jump_requested;
        }

        const auto now = std::chrono::steady_clock::now();
        pending_tics += static_cast<svanes::TicCount>(
            std::chrono::duration_cast<std::chrono::microseconds>(now - previous_tick).count()
        );
        previous_tick = now;

        for (auto &[client_id, character] : characters) {
            svanes::Kinematic2D &motion = world.GetComponent<svanes::Kinematic2D>(character.entity);
            motion.velocity_x = character.horizontal_input * svanes::PerSecondToPerTic(kCharacterMoveSpeedPerSecond);
        }

        while (pending_tics >= svanes::DefaultPhysicsStepTics) {
            svanes::AdvanceTimelines(world, svanes::DefaultPhysicsStepTics);
            svanes::AdvancePhysics(world, gravity, driver, [&](std::span<const svanes::PhysicsTimeStep>) {
                for (auto &[client_id, character] : characters) {
                    svanes::Transform &transform = world.GetComponent<svanes::Transform>(character.entity);
                    transform.x = std::clamp(transform.x, 0.0F, kChrisWorldWidth);
                    transform.y = std::clamp(transform.y, 0.0F, kChrisWorldHeight);

                    ResolveCharacterAxis(world, character.entity, false);
                    ResolveCharacterAxis(world, character.entity, true);

                    svanes::Kinematic2D &motion = world.GetComponent<svanes::Kinematic2D>(character.entity);
                    const bool is_grounded = motion.velocity_y == 0.0F;
                    if (is_grounded && character.jump_requested) {
                        motion.velocity_y = -svanes::PerSecondToPerTic(kJumpSpeedPerSecond);
                    }
                    character.jump_requested = false;
                }
            });
            pending_tics -= svanes::DefaultPhysicsStepTics;
        }

        for (const svanes::EntityTransformState &state : svanes::CollectTransformStates(world)) {
            network_server.Broadcast(state);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}
