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

namespace {

constexpr float kCharacterMoveSpeedPerSecond = 360.0F;
constexpr float kJumpSpeedPerSecond = 800.0F;
constexpr float kGravityPerSecondSquared = 1500.0F;

constexpr float kCharacterSpawnLeft = 64.0F;
constexpr float kCharacterSpawnTop = 64.0F;

constexpr std::int32_t kMaxResolutionIterations = 4;

constexpr float kPlatformSlideDistance = 360.0F;
constexpr float kPlatformSlideSpeedPerSecond = 1400.0F;
constexpr float kPlatformBeatSeconds = 0.5F;
constexpr std::int32_t kPlatformHoldBeats = 2;

struct ServerCharacter {
    svanes::Entity entity;
    svanes::ClientId client_id;
    float horizontal_input = 0.0F;
    bool jump_requested = false;
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

void AdvancePlatform(
    svanes::Registry &world, ServerPlatform &platform, bool trigger_requested, svanes::TicCount hold_tics
) {
    svanes::Transform &transform = world.GetComponent<svanes::Transform>(platform.entity);
    svanes::Kinematic2D &motion = world.GetComponent<svanes::Kinematic2D>(platform.entity);

    switch (platform.state) {
    case PlatformState::Idle:
        if (trigger_requested) {
            platform.state = PlatformState::SlidingLeft;
            motion.velocity_x = -svanes::PerSecondToPerTic(kPlatformSlideSpeedPerSecond);
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
        platform.holding_tics_remaining -=
            std::min(platform.holding_tics_remaining, svanes::DefaultPhysicsStepTics);
        if (platform.holding_tics_remaining == 0) {
            platform.state = PlatformState::SlidingBack;
            motion.velocity_x = svanes::PerSecondToPerTic(kPlatformSlideSpeedPerSecond);
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
    svanes::NetworkServer network_server(kChrisStatePort, kChrisInputPort);

    svanes::Registry world;

    const float ground_top = kChrisWorldHeight - kChrisGroundHeight;
    const float platform_top = ground_top - kChrisPlatformTopHeightAboveGround;
    const float platform_home_x = kChrisPlatformLeft + kChrisPlatformWidth * 0.5F;

    const svanes::Entity platform_entity = world.CreateEntity();
    if (platform_entity != kChrisPlatformNetworkEntity) {
        SDL_Log(
            "Server: expected the platform to be entity %u but entity creation order produced %u instead.",
            kChrisPlatformNetworkEntity, platform_entity
        );
        return 1;
    }
    world.AddComponent<svanes::Transform>(
        platform_entity, svanes::Transform{platform_home_x, platform_top + kChrisPlatformHeight * 0.5F, 0.0F}
    );
    world.AddComponent<svanes::Collider2D>(
        platform_entity, svanes::Collider2D{svanes::Rectangle2D{0.0F, 0.0F, kChrisPlatformWidth, kChrisPlatformHeight}}
    );
    world.AddComponent<svanes::Kinematic2D>(platform_entity, svanes::Kinematic2D{});
    world.AddComponent<svanes::Timeline>(platform_entity);
    world.AddComponent<svanes::Networked>(platform_entity);

    ServerPlatform platform{platform_entity, platform_home_x, platform_home_x - kPlatformSlideDistance};
    const svanes::TicCount platform_hold_tics = svanes::SecondsToTics(kPlatformBeatSeconds * kPlatformHoldBeats);

    const svanes::Entity ground_entity = world.CreateEntity();
    world.AddComponent<svanes::Transform>(
        ground_entity, svanes::Transform{kChrisWorldWidth * 0.5F, ground_top + kChrisGroundHeight * 0.5F, 0.0F}
    );
    world.AddComponent<svanes::Collider2D>(
        ground_entity, svanes::Collider2D{svanes::Rectangle2D{0.0F, 0.0F, kChrisWorldWidth, kChrisGroundHeight}}
    );

    std::optional<ServerCharacter> character;
    bool platform_trigger_requested = false;

    svanes::AsyncParallelForDriver driver(1);
    const svanes::Vector2D gravity{0.0F, svanes::PerSecondSquaredToPerTicSquared(kGravityPerSecondSquared)};

    auto previous_tick = std::chrono::steady_clock::now();
    svanes::TicCount pending_tics = 0;

    while (true) {
        for (const svanes::NetworkMessage &message : network_server.PollInbound()) {
            const PlayerInputMessage input = message.As<PlayerInputMessage>();

            if (input.role == ClientRole::Platform) {
                platform_trigger_requested = platform_trigger_requested || input.action_requested;
                continue;
            }

            if (!character.has_value()) {
                const svanes::Entity entity = SpawnCharacter(world, kCharacterSpawnLeft, kCharacterSpawnTop);
                character = ServerCharacter{entity, input.client_id};
                SDL_Log("Server: spawned the character for client %u.", input.client_id);
            } else if (character->client_id != input.client_id) {
                SDL_Log(
                    "Server: client %u took control of the character from client %u.", input.client_id,
                    character->client_id
                );
                character->client_id = input.client_id;
            }

            character->horizontal_input = input.horizontal;
            character->jump_requested = character->jump_requested || input.action_requested;
        }

        const auto now = std::chrono::steady_clock::now();
        pending_tics += static_cast<svanes::TicCount>(
            std::chrono::duration_cast<std::chrono::microseconds>(now - previous_tick).count()
        );
        previous_tick = now;

        if (character.has_value()) {
            svanes::Kinematic2D &motion = world.GetComponent<svanes::Kinematic2D>(character->entity);
            motion.velocity_x = character->horizontal_input * svanes::PerSecondToPerTic(kCharacterMoveSpeedPerSecond);
        }

        while (pending_tics >= svanes::DefaultPhysicsStepTics) {
            svanes::AdvanceTimelines(world, svanes::DefaultPhysicsStepTics);
            svanes::AdvancePhysics(world, gravity, driver, [&](std::span<const svanes::PhysicsTimeStep>) {
                if (character.has_value()) {
                    svanes::Transform &transform = world.GetComponent<svanes::Transform>(character->entity);
                    transform.x = std::clamp(transform.x, 0.0F, kChrisWorldWidth);
                    transform.y = std::clamp(transform.y, 0.0F, kChrisWorldHeight);

                    ResolveCharacterAxis(world, character->entity, false);
                    ResolveCharacterAxis(world, character->entity, true);

                    svanes::Kinematic2D &motion = world.GetComponent<svanes::Kinematic2D>(character->entity);
                    const bool is_grounded = motion.velocity_y == 0.0F;
                    if (is_grounded && character->jump_requested) {
                        motion.velocity_y = -svanes::PerSecondToPerTic(kJumpSpeedPerSecond);
                    }
                    character->jump_requested = false;
                }

                AdvancePlatform(world, platform, platform_trigger_requested, platform_hold_tics);
                platform_trigger_requested = false;
            });
            pending_tics -= svanes::DefaultPhysicsStepTics;

            for (const svanes::EntityTransformState &state : svanes::CollectTransformStates(world)) {
                network_server.Broadcast(state);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
