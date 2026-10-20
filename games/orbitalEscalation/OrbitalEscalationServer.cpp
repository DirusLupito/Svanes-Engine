#include "OrbitalEscalationServer.hpp"

#include "attachment_system.hpp"
#include "combat/damage_system.hpp"
#include "combat/missile_system.hpp"
#include "effects/collision_flashes.hpp"

#include <svanes/kinematic_system.hpp>
#include <svanes/network/message_serialization.hpp>
#include <svanes/sprite_animation_system.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <utility>

constexpr float kPlanetRadius = 4200.0F;
constexpr float kPlayerHealth = 100.0F;
constexpr float kPlanetImpactDamage = 25.0F;
constexpr float kPlanetAttraction = 180000000.0F;
constexpr float kCollisionAcceleration = 400000.0F;
constexpr float kSpawnSpacing = 2000.0F;
constexpr float kSpawnHeight = 48000.0F;
constexpr float kSpawnSpeed = 300.0F;

/**
 * Helper for the planet's gravitational field.
 * Returns the acceleration vector at a given offset from the planet's center.
 *
 * @param offset_to_source The offset vector from the planet's center to the
 * point of interest.
 * @return The acceleration vector at the given offset, pointing towards the
 * planet's center.
 */
static svanes::Vector2D AttractionField(svanes::Vector2D offset_to_source) {
    const float distance = std::hypot(offset_to_source.x, offset_to_source.y);

    if (distance == 0.0F) {
        return {};
    }

    const float strength =
        svanes::PerSecondSquaredToPerTicSquared(kPlanetAttraction) /
        (1.0F + distance * distance / kPlanetRadius);

    return offset_to_source / distance * strength;
}

/**
 * Creates a desert planet layer with a given radius, color, and z-order.
 *
 * @param radius The radius of the planet layer.
 * @param color The color of the planet layer.
 * @param z_order The z-order of the planet layer for rendering.
 *
 * @return The visual representing the planet layer.
 */
static Visual CreatePlanetLayer(float radius, svanes::Color color,
                                std::int32_t z_order) {
    return {svanes::SolidShape{color, svanes::Circle2D{0.0F, 0.0F, radius}},
            z_order};
}

/**
 * Applies an acceleration to two entities based on their collision, if they
 * have collided to slam them apart. The acceleration is applied in the
 * direction of the collision normal.
 *
 * @param world The registry containing the entities.
 * @param pair The entities and collision details from the shared detection
 * pass.
 */
static void ApplyCollisionAcceleration(svanes::Registry &world,
                                       const svanes::EntityCollision2D &pair) {
    const auto a = pair.a;
    const auto b = pair.b;

    for (const svanes::Collision2D &collision : pair.collisions) {
        const svanes::Vector2D acceleration =
            collision.normal *
            svanes::PerSecondSquaredToPerTicSquared(kCollisionAcceleration);

        if (world.HasComponent<svanes::Kinematic2D>(a)) {
            auto &motion = world.GetComponent<svanes::Kinematic2D>(a);
            motion.acceleration_x += acceleration.x;
            motion.acceleration_y += acceleration.y;
        }

        if (world.HasComponent<svanes::Kinematic2D>(b)) {
            auto &motion = world.GetComponent<svanes::Kinematic2D>(b);
            motion.acceleration_x -= acceleration.x;
            motion.acceleration_y -= acceleration.y;
        }
    }
}

OrbitalEscalationServer::OrbitalEscalationServer(
    const std::filesystem::path &asset_root, std::uint16_t joining_port)
    : assets(asset_root), port(joining_port) {}

void OrbitalEscalationServer::Initialize(svanes::ServerContext &context) {
    gameplay_timeline.emplace(context.world);
    context.world.AddComponent<svanes::Timeline>(gameplay_timeline->Get());
    planet.emplace(context.world, MakePlanetDefinition());
    planet->GetTransform(context.world) = {0.0F, 0.0F};
    context.network.Listen("*:" + std::to_string(port));

    std::cout << "Orbital Escalation server: UDP port " << port << '.'
              << std::endl;
}

svanes::JoinResponse OrbitalEscalationServer::HandleJoinRequest(
    const svanes::ServerJoinContext &context) {
    AddPlayer(context.world, context.client);
    std::cout << "Player " << context.client << " joined.\n";
    return {true, {}};
}

OrbitalEscalationServer::Input
OrbitalEscalationServer::DecodeInput(const svanes::NetworkMessage &message) {
    svanes::MessageReader reader(message);
    Input input;
    input.propulsion = {{reader.ReadFloat32(), reader.ReadFloat32()},
                        reader.ReadFloat32()};

    input.weapons.fire = reader.ReadBool();
    input.pause = reader.ReadBool();

    for (float value : {input.propulsion.thrust.x, input.propulsion.thrust.y,
                        input.propulsion.rotation}) {
        if (!std::isfinite(value) || std::abs(value) > 1.0F) {
            throw std::invalid_argument("Invalid Orbital propulsion control.");
        }
    }
    return input;
}

void OrbitalEscalationServer::Update(
    const svanes::ServerUpdateContext &context) {
    for (const auto &message : context.network.PollInputs()) {
        const auto input = DecodeInput(message.message);
        auto &held = players.at(message.client).input;
        held.propulsion = input.propulsion;
        held.weapons.fire |= input.weapons.fire;
        held.pause |= input.pause;
    }

    bool toggle_pause = false;
    for (auto &[client, player] : players) {
        toggle_pause |= player.input.pause;
        player.input.pause = false;
    }

    if (toggle_pause) {
        auto &timeline = context.world.GetComponent<svanes::Timeline>(
            gameplay_timeline->Get());
        if (timeline.IsPaused()) {
            timeline.Unpause();
        } else {
            timeline.Pause();
        }
    }
}

void OrbitalEscalationServer::PhysicsUpdate(
    const svanes::ServerPhysicsContext &context) {
    auto &world = context.world;

    if (!world.GetComponent<svanes::Timeline>(gameplay_timeline->Get())
             .IsPaused()) {
        RespondToPhysics(world, context.parallel_for);
        for (auto &[client, player] : players) {
            if (player.ship) {
                SetWeaponControls(world, *player.ship, player.input.weapons);
                UpdateWeapons(world, *player.ship, assets,
                              gameplay_timeline->Get(), projectiles);
            }
        }

        const auto count = projectiles.size();
        for (std::size_t i = 0; i < count; ++i) {
            UpdateWeapons(world, *projectiles[i], assets,
                          gameplay_timeline->Get(), projectiles);
        }

        UpdateAttachments(world);
    }
    for (auto &[client, player] : players) {
        player.input.weapons.fire = false;
    }

    svanes::AdvanceSpriteAnimations(world);
    UpdateCollisionFlashes(world, collision_flashes);
}

/**
 * Checks whether an object and its attachments fit at their current positions
 * without colliding with the existing world.
 *
 * @param world The registry containing the object and obstacles.
 * @param object The object whose spawn position to check.
 * @param obstacles The existing entities to avoid.
 * @return true if the object and all its attachments are clear of obstacles.
 * @throws std::out_of_range if a checked entity lacks a transform or collider.
 */
static bool SpawnIsClear(const svanes::Registry &world,
                         const DynamicObject &object,
                         std::span<const svanes::Entity> obstacles) {
    const auto entity = object.GetEntity();
    const auto &geometry =
        world.GetComponent<svanes::Collider2D>(entity).geometry;

    const auto &transform = world.GetComponent<svanes::Transform>(entity);
    for (const auto obstacle : obstacles) {
        if (!svanes::DetectCollisions(
                 geometry, transform,
                 world.GetComponent<svanes::Collider2D>(obstacle).geometry,
                 world.GetComponent<svanes::Transform>(obstacle))
                 .empty()) {
            return false;
        }
    }

    for (const auto &child : object.GetAttachments()) {
        if (!SpawnIsClear(world, *child, obstacles)) {
            return false;
        }
    }
    return true;
}

void OrbitalEscalationServer::AddPlayer(svanes::Registry &world,
                                        svanes::ClientId client) {
    if (client == 0 || players.contains(client)) {
        throw std::invalid_argument(
            "Orbital player requires a unique, nonzero client ID.");
    }

    // Collect existing geometry before creating the ship so it cannot block
    // its own spawn position.
    std::vector<svanes::Entity> obstacles;
    world.ForEach<svanes::Transform, svanes::Collider2D>(
        [&](svanes::Entity entity, const auto &, const auto &) {
            obstacles.push_back(entity);
        });

    auto ship =
        assets.CreateShip(world, gameplay_timeline->Get(), "player_ship");
    world.AddComponent<Health>(ship->GetEntity(), Health{kPlayerHealth});

    // Find an available spawn position.
    for (float x = 0.0F;;) {
        ship->GetTransform(world) = {x, -kPlanetRadius - kSpawnHeight};

        // Update the attachments of all objects, so that their transforms are
        // correct for collision detection.
        UpdateAttachments(world);
        if (SpawnIsClear(world, *ship, obstacles)) {
            break;
        }

        const float next = x + kSpawnSpacing;
        if (!std::isfinite(next) || next == x) {
            throw std::overflow_error("Orbital spawn positions exhausted.");
        }

        x = next;
    }

    ship->GetKinematic(world).velocity_x =
        svanes::PerSecondToPerTic(kSpawnSpeed);
    players.emplace(client, Player{std::move(ship)});
    UpdateAttachments(world);
}

void OrbitalEscalationServer::RespondToPhysics(
    svanes::Registry &world, svanes::AsyncParallelForDriver &driver) {

    for (auto &[client, player] : players) {
        if (player.ship) {
            world.GetComponent<PropulsionControl>(player.ship->GetEntity()) =
                player.input.propulsion;
        }
    }
    ApplyPropulsion(world);

    // Physics moved the roots of the attachment trees. Bring their children
    // to the new positions before checking for collisions.
    UpdateAttachments(world);

    std::vector<svanes::Entity> colliders;
    world.ForEach<svanes::Collider2D, svanes::Transform>(
        [&](svanes::Entity entity, const auto &, const auto &) {
            if (!IsDead(world, entity)) {
                colliders.push_back(entity);
            }
        });
    auto contacts = svanes::DetectEntityCollisions(world, colliders, driver);

    // Parts of the same object should not collide with each other. An arming
    // missile also ignores its firing ship and that ship's attached parts.
    std::erase_if(contacts, [&](const svanes::EntityCollision2D &pair) {
        return GetAttachmentRoot(world, pair.a) ==
                   GetAttachmentRoot(world, pair.b) ||
               IgnoresFiringShip(world, pair.a, pair.b) ||
               IgnoresFiringShip(world, pair.b, pair.a);
    });

    for (const auto &pair : contacts) {
        ApplyCollisionAcceleration(world, pair);
    }

    // For every player, check if they're contacting the planet.
    for (auto &[client, player] : players) {
        bool touching_planet = false;
        if (player.ship && !IsDead(world, player.ship->GetEntity())) {
            const auto entity = player.ship->GetEntity();
            const auto surface = planet->GetEntity();
            for (const auto &pair : contacts) {
                if ((pair.a == entity && pair.b == surface) ||
                    (pair.b == entity && pair.a == surface)) {
                    touching_planet = true;
                }
            }

            // Keep pushing the ship away from the surface, but only deal
            // impact damage when contact begins. Otherwise sitting against
            // the planet would deal damage on every tick.
            if (touching_planet && !player.touching_planet) {
                ApplyDamage(world, entity, kPlanetImpactDamage);
            }
        }
        player.touching_planet = touching_planet;
    }

    UpdateMissileExplosions(world, gameplay_timeline->Get(), contacts,
                            collision_flashes);

    // Explosions have finished using the entities' geometry. Now their owners
    // can remove dead objects, including dead missiles still inside launchers.
    // A player's roster entry remains even when their ship is removed.
    for (auto &[client, player] : players) {
        if (player.ship) {
            if (IsDead(world, player.ship->GetEntity())) {
                player.ship.reset();
            } else {
                player.ship->RemoveDeadAttachments(world);
            }
        }
    }

    std::erase_if(projectiles, [&](const auto &object) {
        if (IsDead(world, object->GetEntity())) {
            return true;
        }
        object->RemoveDeadAttachments(world);
        return false;
    });
}


PlanetDefinition OrbitalEscalationServer::MakePlanetDefinition() {
    return PlanetDefinition{
        .attractor = {.accelerationField = AttractionField,
                      .cutoff_radius = std::nullopt,
                      .allow_parallel = true},
        .collider = {svanes::Circle2D{0.0F, 0.0F, kPlanetRadius}},
        .visuals = {CreatePlanetLayer(kPlanetRadius, {255, 127, 38, 255}, -3),
                    CreatePlanetLayer(3900.0F, {185, 122, 87, 255}, -2),
                    CreatePlanetLayer(3750.0F, {127, 127, 127, 255}, -1)},
    };
}
