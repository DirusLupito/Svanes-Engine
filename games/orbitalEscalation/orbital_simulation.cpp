#include "orbital_simulation.hpp"
#include "attachment_system.hpp"
#include "combat/damage_system.hpp"
#include "combat/missile_system.hpp"
#include "combat/weapon_system.hpp"
#include "controls.hpp"
#include "effects/collision_flashes.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <svanes/input.hpp>
#include <svanes/kinematic_system.hpp>
#include <svanes/registry.hpp>
#include <svanes/sprite_animation_system.hpp>
#include <svanes/stable_id.hpp>
#include <utility>

constexpr float kPlanetRadius = 4200.0F;
constexpr float kPlayerHealth = 100.0F;
constexpr float kPlanetImpactDamage = 25.0F;

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
        svanes::PerSecondSquaredToPerTicSquared(180000000.0F) /
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
            svanes::PerSecondSquaredToPerTicSquared(400000.0F);
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

OrbitalSimulation::OrbitalSimulation(svanes::Registry &world,
                                     const std::filesystem::path &asset_root,
                                     std::uint32_t concurrency)
    : world(world), driver(concurrency), gameplay_timeline(world),
      assets(asset_root) {
    // Create the overarching gameplay timeline entity, which we can use
    // to pause all gameplay, or speedup/slowdown all gameplay.
    world.AddComponent<svanes::Timeline>(gameplay_timeline.Get());
    AssignIdentity(gameplay_timeline.Get());
    planet.emplace(
        world,
        PlanetDefinition{
            .attractor = {.accelerationField = AttractionField,
                          .cutoff_radius = std::nullopt,
                          .allow_parallel = true},
            .collider = {svanes::Circle2D{0.0F, 0.0F, kPlanetRadius}},
            .visuals = {CreatePlanetLayer(kPlanetRadius, {255, 127, 38, 255},
                                          -3),
                        CreatePlanetLayer(3900.0F, {185, 122, 87, 255}, -2),
                        CreatePlanetLayer(3750.0F, {127, 127, 127, 255}, -1)},
        });
    planet->GetTransform(world) = {0.0F, 0.0F};
    planet->UpdateVisuals(world);

    AssignIdentity(planet->GetEntity());
}

void OrbitalSimulation::AssignIdentity(svanes::Entity entity) {

    // no need, entity already has a stable id
    if (world.HasComponent<svanes::StableId>(entity)) {
        return;
    }

    if (next_identity == std::numeric_limits<std::uint64_t>::max()) {
        throw std::overflow_error(
            "Orbital shared object identities exhausted.");
    }

    world.AddComponent<svanes::StableId>(entity,
                                         svanes::StableId{next_identity++});
}

void OrbitalSimulation::AssignIdentities(const DynamicObject &object) {
    AssignIdentity(object.GetEntity());

    // Attachments can become detached independent objects. So they
    // also need to be stable across clients.

    for (const auto &child : object.GetAttachments()) {
        AssignIdentities(*child);
    }
}

void OrbitalSimulation::AddPlayer(svanes::PeerId peer) {
    if (peer.value == 0 || players.contains(peer.value)) {
        throw std::invalid_argument(
            "Orbital player requires a unique, nonzero peer ID.");
    }

    auto ship =
        assets.CreateShip(world, gameplay_timeline.Get(), "player_ship");
    world.AddComponent<Health>(ship->GetEntity(), Health{kPlayerHealth});
    ship->GetTransform(world) = {static_cast<float>(peer.value - 1) * 2000.0F,
                                 -kPlanetRadius - 48000.0F};

    ship->GetKinematic(world).velocity_x = svanes::PerSecondToPerTic(300.0F);
    AssignIdentities(*ship);
    players.emplace(peer.value, Player{std::move(ship)});
    UpdateAttachments(world);
    UpdateVisuals();
}

void OrbitalSimulation::RemovePlayer(svanes::PeerId peer) {
    if (players.erase(peer.value) == 0) {
        throw std::invalid_argument("Orbital player is not in the roster.");
    }
}

std::optional<svanes::Entity>
OrbitalSimulation::PlayerEntity(svanes::PeerId peer) const {
    const auto found = players.find(peer.value);

    // Being in the game does not necessarily mean the player still has a ship.
    if (found == players.end() || !found->second.ship) {
        return std::nullopt;
    }

    return found->second.ship->GetEntity();
}

void OrbitalSimulation::CaptureInput(const svanes::FrameContext &frame,
                                     svanes::PeerId peer) {
    // Mouse steering uses this player's ship and local camera to produce a
    // propulsion request. Every peer then applies that same request.
    OrbitalInput input;
    if (const auto entity = PlayerEntity(peer)) {
        input.propulsion =
            ReadShipControls(frame.input, frame.camera,
                             world.GetComponent<svanes::Transform>(*entity),
                             world.GetComponent<svanes::Kinematic2D>(*entity),
                             world.GetComponent<Propulsion>(*entity));
        input.weapons.fire =
            frame.input.WasMouseButtonPressed(svanes::MouseButton::Right);
    }

    // If the player inputs between ticks, later frames must not overwrite
    // that input with false at least until TakeInput gets around to it.

    input.weapons.fire = input.weapons.fire || pending_input.weapons.fire;
    input.pause = frame.input.WasPressed(svanes::Key::P) || pending_input.pause;
    pending_input = input;
}

OrbitalInput OrbitalSimulation::TakeInput() {
    const auto input = pending_input;

    // Now we've actually gotten to handle this frames input,
    // so we reset so that key presses aren't toggles/locked to true forever.
    
    pending_input.weapons.fire = false;
    pending_input.pause = false;
    return input;
}

/**
 * Checks that propulsion requests are finite and within the control range.
 * Used before sending controls or applying them to the simulation.
 *
 * @param input The controls to validate.
 *
 * @throws std::invalid_argument if any propulsion value is nonfinite or
 * outside [-1, 1].
 */
static void ValidateInput(const OrbitalInput &input) {
    for (float value : {input.propulsion.thrust.x, input.propulsion.thrust.y,
                        input.propulsion.rotation}) {
        if (!std::isfinite(value) || std::abs(value) > 1.0F) {
            throw std::invalid_argument("Invalid Orbital propulsion control.");
        }
    }
}

void OrbitalSimulation::EncodeInput(svanes::MessageWriter &writer,
                                    const OrbitalInput &input) const {
    ValidateInput(input);

    // Write each field explicitly so the message does not depend on struct
    // padding or the byte order used by the sending computer.
    writer.WriteFloat32(input.propulsion.thrust.x);
    writer.WriteFloat32(input.propulsion.thrust.y);
    writer.WriteFloat32(input.propulsion.rotation);
    writer.WriteBool(input.weapons.fire);
    writer.WriteBool(input.pause);
}

OrbitalInput
OrbitalSimulation::DecodeInput(svanes::MessageReader &reader) const {
    OrbitalInput input;
    input.propulsion = {{reader.ReadFloat32(), reader.ReadFloat32()},
                        reader.ReadFloat32()};
    input.weapons.fire = reader.ReadBool();
    input.pause = reader.ReadBool();

    // messages went through the encoder but it doesnt hurt (much) to be safe
    ValidateInput(input);
    return input;
}

bool OrbitalSimulation::IsPaused() const {
    return world.GetComponent<svanes::Timeline>(gameplay_timeline.Get())
        .IsPaused();
}

void OrbitalSimulation::Step(std::span<const OrbitalInput> inputs) {
    if (inputs.size() != players.size()) {
        throw std::invalid_argument("Orbital tick needs one input per player.");
    }
    for (const auto &input : inputs) {
        ValidateInput(input);
    }

    // Currently, attractors are collected in registry order, which can differ between
    // peers. With several attractors, even adding the same forces in a
    // different order can change the floating point result. One source avoids
    // that problem for now.

    std::size_t attractors = 0;
    world.ForEach<svanes::PointAttractor2D>(
        [&](svanes::Entity, const auto &) { ++attractors; });

    // TODO: support multiple attractors in multiplayer.
    if (attractors != 1) {
        throw std::logic_error("Orbital synchronized simulation currently "
                               "requires one attractor.");
    }

    // If two players press pause on the same tick, we should pause once.
    // Toggling separately for each player would just cancel their requests.
    // This also has to run while paused, so we can unpause the game.
    if (std::any_of(inputs.begin(), inputs.end(),
                    [](const auto &input) { return input.pause; })) {
        auto &timeline =
            world.GetComponent<svanes::Timeline>(gameplay_timeline.Get());
        if (timeline.IsPaused()) {
            timeline.Unpause();
        } else {
            timeline.Pause();
        }
    }

    // Advance time once for the whole tick, including child timelines.
    // Movement uses the previous tick's acceleration. RespondToPhysics then
    // applies controls and resolves collisions, explosions, and destruction.
    // Only after that can surviving weapons fire. Newly fired missiles first
    // move on the next tick.

    svanes::AdvanceTimelines(world, OrbitalStepTics);
    if (!IsPaused()) {
        svanes::AdvancePhysics(world, {}, driver,
                               [&](auto) { RespondToPhysics(inputs); });
        std::size_t index = 0;

        // Right click requests one shot from every weapon on the player ship.
        // The weapon system decides which launchers have ammunition and are
        // ready.

        for (auto &[peer, player] : players) {
            if (player.ship) {
                SetWeaponControls(world, *player.ship, inputs[index].weapons);
                UpdateWeapons(world, *player.ship, assets,
                              gameplay_timeline.Get(), projectiles);
            }
            ++index;
        }

        // Updating a projectile may launch another projectile, extending this
        // vector. We'll only update the weapons that existed initially this step.
    
        const auto count = projectiles.size();
        for (std::size_t i = 0; i < count; ++i) {
            UpdateWeapons(world, *projectiles[i], assets,
                          gameplay_timeline.Get(), projectiles);
        }

        // Reloading may have created new ammunition. Assign its identity in
        // shared object order before the next collision pass can use it.

        for (const auto &[peer, player] : players) {
            if (player.ship) {
                AssignIdentities(*player.ship);
            }
        }

        // Also just in case, assign identities to anything else.
        for (const auto &object : projectiles) {
            AssignIdentities(*object);
        }

        UpdateAttachments(world);
    }

    svanes::AdvanceSpriteAnimations(world);
    UpdateCollisionFlashes(world, collision_flashes);
}

void OrbitalSimulation::RespondToPhysics(std::span<const OrbitalInput> inputs) {

    // A dead player can still occupy an input slot.

    std::size_t index = 0;
    for (auto &[peer, player] : players) {
        if (player.ship) {
            const auto entity = player.ship->GetEntity();
            world.GetComponent<PropulsionControl>(entity) =
                inputs[index].propulsion;
        }
        ++index;
    }
    ApplyPropulsion(world);

    // Physics moved the roots of the attachment trees. Bring their children
    // to the new positions before checking for collisions.
    UpdateAttachments(world);

    // Local entity IDs can differ between peers. Shared order determines both
    // which object goes first in a collision test and the order of responses.
    // This matters when several axes have the same penetration
    // depth since swapping the objects can change which normal is chosen.

    auto colliders =
        svanes::EntitiesByStableId<svanes::Collider2D, svanes::Transform>(
            world);

    std::erase_if(colliders,
                  [&](auto entity) { return IsDead(world, entity); });

    auto contacts = svanes::DetectEntityCollisions(
        world, colliders, driver, 16, svanes::CollisionOrder::InputOrder);

    // Parts of the same object should not collide with each other. An arming
    // missile also ignores its firing ship and that ship's attached parts.
    std::erase_if(contacts, [&](const svanes::EntityCollision2D &pair) {
        return GetAttachmentRoot(world, pair.a) ==
                   GetAttachmentRoot(world, pair.b) ||
               IgnoresFiringShip(world, pair.a, pair.b) ||
               IgnoresFiringShip(world, pair.b, pair.a);
    });

    // For every player, check if they're contacting the planet.
    for (auto &[peer, player] : players) {
        bool touching_planet = false;
        if (player.ship && !IsDead(world, player.ship->GetEntity())) {
            const auto entity = player.ship->GetEntity();
            const auto surface = planet->GetEntity();
            for (const auto &pair : contacts) {
                if ((pair.a == entity && pair.b == surface) ||
                    (pair.b == entity && pair.a == surface)) {
                    touching_planet = true;
                    ApplyCollisionAcceleration(world, pair);
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

    UpdateMissileExplosions(world, gameplay_timeline.Get(), contacts,
                            collision_flashes);

    // Explosions have finished using the entities' geometry. Now their owners
    // can remove dead objects, including dead missiles still inside launchers.
    // A player's roster entry remains even when their ship is removed.
    for (auto &[peer, player] : players) {
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


void OrbitalSimulation::UpdateVisuals() {
    // Visuals are entities that just have one single visual component, like a
    // sprite or a solid shape. So even though the engine will update the ship
    // entity and the planet entity, we need to update their visuals separately
    // to make sure they are drawn correctly on the screen.
    for (const auto &[peer, player] : players) {
        if (player.ship) {
            player.ship->UpdateVisuals(world);
        }
    }

    for (const auto &object : projectiles) {
        object->UpdateVisuals(world);
    }

    planet->UpdateVisuals(world);
}
