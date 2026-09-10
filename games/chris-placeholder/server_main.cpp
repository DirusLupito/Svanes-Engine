#include "network_protocol.hpp"

#include <svanes/collision_system.hpp>
#include <svanes/kinematic_system.hpp>
#include <svanes/registry.hpp>
#include <svanes/vector2d.hpp>

#include <zmq.hpp>

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <optional>
#include <thread>

namespace {

constexpr float kCharacterMoveSpeed = 360.0F;
constexpr float kShapeMoveSpeed = 360.0F;
constexpr float kJumpSpeed = 800.0F;
constexpr svanes::Vector2D kGravity{0.0F, 1500.0F};

constexpr std::int32_t kMaxResolutionIterations = 4;

/**
 * Corrects the character's position out of any overlapping collider along a single axis,
 * zeroing the corresponding velocity component on contact. Run once per axis per tick.
 */
void ResolveCharacterAxis(svanes::Registry& world, svanes::Entity character_entity, bool horizontal)
{
    svanes::Transform& character_transform = world.GetComponent<svanes::Transform>(character_entity);
    const svanes::Collider2D& character_collider = world.GetComponent<svanes::Collider2D>(character_entity);
    svanes::Kinematic2D& character_motion = world.GetComponent<svanes::Kinematic2D>(character_entity);

    for (std::int32_t iteration = 0; iteration < kMaxResolutionIterations; ++iteration) {
        std::optional<svanes::Collision2D> deepest_collision;

        world.ForEach<svanes::Transform, svanes::Collider2D>(
            [&](svanes::Entity candidate_entity, const svanes::Transform& candidate_transform, const svanes::Collider2D& candidate_collider) {
                if (candidate_entity == character_entity) {
                    return;
                }

                for (const svanes::Collision2D& collision : svanes::DetectCollisions(
                         character_collider.geometry, character_transform,
                         candidate_collider.geometry, candidate_transform
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

/**
 * Finds the entity state a client's input should apply to, assigning the client to
 * the character or the shape the first time it is seen. Returns std::nullopt once both
 * roles are already taken by other clients.
 */
std::optional<svanes::Entity> ResolveControlledEntity(
    std::uint32_t client_id,
    std::optional<std::uint32_t>& character_client_id,
    std::optional<std::uint32_t>& shape_client_id,
    svanes::Entity character_entity,
    svanes::Entity shape_entity
)
{
    if (character_client_id == client_id) {
        return character_entity;
    }
    if (shape_client_id == client_id) {
        return shape_entity;
    }
    if (!character_client_id.has_value()) {
        character_client_id = client_id;
        SDL_Log("Server: assigned client %u to the character.", client_id);
        return character_entity;
    }
    if (!shape_client_id.has_value()) {
        shape_client_id = client_id;
        SDL_Log("Server: assigned client %u to the shape.", client_id);
        return shape_entity;
    }
    return std::nullopt;
}

}

int32_t main()
{
    zmq::context_t context;

    zmq::socket_t state_socket(context, zmq::socket_type::pub);
    state_socket.bind(ChrisServerEndpoint("*", kChrisStatePort));
    SDL_Log("Server: bound state broadcast socket on port %u.", kChrisStatePort);

    zmq::socket_t input_socket(context, zmq::socket_type::pull);
    input_socket.bind(ChrisServerEndpoint("*", kChrisInputPort));
    SDL_Log("Server: bound input socket on port %u.", kChrisInputPort);

    const auto beat_clock_start = std::chrono::steady_clock::now();

    svanes::Registry world;

    const svanes::Entity character_entity = world.CreateEntity();
    world.AddComponent<svanes::Transform>(
        character_entity, svanes::Transform{kChrisCharacterWidth, kChrisCharacterHeight, 0.0F}
    );
    world.AddComponent<svanes::Kinematic2D>(character_entity, svanes::Kinematic2D{});
    world.AddComponent<svanes::Gravity>(character_entity, svanes::Gravity{});
    world.AddComponent<svanes::Collider2D>(
        character_entity, svanes::Collider2D{svanes::Rectangle2D{0.0F, 0.0F, kChrisCharacterWidth, kChrisCharacterHeight}}
    );

    const svanes::Entity shape_entity = world.CreateEntity();
    world.AddComponent<svanes::Transform>(
        shape_entity, svanes::Transform{kChrisWorldWidth * 0.5F, kChrisWorldHeight * 0.5F, 0.0F}
    );
    world.AddComponent<svanes::Collider2D>(
        shape_entity, svanes::Collider2D{svanes::Rectangle2D{0.0F, 0.0F, kChrisShapeSize, kChrisShapeSize}}
    );

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

    std::optional<std::uint32_t> character_client_id;
    std::optional<std::uint32_t> shape_client_id;

    float character_horizontal_input = 0.0F;
    bool character_jump_requested = false;
    float shape_horizontal_input = 0.0F;
    float shape_vertical_input = 0.0F;

    auto previous_tick = std::chrono::steady_clock::now();
    float beat_sync_timer = 0.0F;

    while (true) {
        while (true) {
            zmq::message_t message;
            const zmq::recv_result_t result = input_socket.recv(message, zmq::recv_flags::dontwait);
            if (!result.has_value()) {
                break;
            }

            if (message.size() != sizeof(PlayerInputMessage)) {
                SDL_Log("Server: received unexpected input packet of size %zu.", message.size());
                continue;
            }

            const PlayerInputMessage& input = *message.data<PlayerInputMessage>();
            const std::optional<svanes::Entity> controlled_entity = ResolveControlledEntity(
                input.client_id, character_client_id, shape_client_id, character_entity, shape_entity
            );

            if (controlled_entity == character_entity) {
                character_horizontal_input = input.horizontal;
                character_jump_requested = character_jump_requested || input.jump_requested;
            } else if (controlled_entity == shape_entity) {
                shape_horizontal_input = input.horizontal;
                shape_vertical_input = input.vertical;
            }
        }

        const auto now = std::chrono::steady_clock::now();
        const float delta_seconds = std::chrono::duration<float>(now - previous_tick).count();
        previous_tick = now;

        svanes::Transform& shape_transform = world.GetComponent<svanes::Transform>(shape_entity);
        shape_transform.x = std::clamp(
            shape_transform.x + shape_horizontal_input * kShapeMoveSpeed * delta_seconds, 0.0F, kChrisWorldWidth
        );
        shape_transform.y = std::clamp(
            shape_transform.y + shape_vertical_input * kShapeMoveSpeed * delta_seconds, 0.0F, kChrisWorldHeight
        );

        svanes::Kinematic2D& character_motion = world.GetComponent<svanes::Kinematic2D>(character_entity);
        character_motion.velocity_x = character_horizontal_input * kCharacterMoveSpeed;

        svanes::AdvanceKinematics(world, delta_seconds, kGravity);

        svanes::Transform& character_transform = world.GetComponent<svanes::Transform>(character_entity);
        character_transform.x = std::clamp(character_transform.x, 0.0F, kChrisWorldWidth);
        character_transform.y = std::clamp(character_transform.y, 0.0F, kChrisWorldHeight);

        ResolveCharacterAxis(world, character_entity, false);
        ResolveCharacterAxis(world, character_entity, true);

        const bool is_grounded = character_motion.velocity_y == 0.0F;
        if (is_grounded && character_jump_requested) {
            character_motion.velocity_y = -kJumpSpeed;
        }
        character_jump_requested = false;

        const GameStateMessage state_message{
            character_transform.x, character_transform.y, shape_transform.x, shape_transform.y
        };
        state_socket.send(zmq::buffer(&state_message, sizeof(state_message)), zmq::send_flags::dontwait);

        beat_sync_timer += delta_seconds;
        if (beat_sync_timer >= kChrisBeatSyncIntervalSeconds) {
            beat_sync_timer -= kChrisBeatSyncIntervalSeconds;

            const BeatSyncMessage beat_message{
                std::chrono::duration<float>(now - beat_clock_start).count()
            };
            state_socket.send(zmq::buffer(&beat_message, sizeof(beat_message)), zmq::send_flags::dontwait);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}
