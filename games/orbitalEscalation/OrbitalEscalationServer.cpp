#include "OrbitalEscalationServer.hpp"

#include "attachment_system.hpp"
#include "combat/damage_system.hpp"
#include "combat/missile_system.hpp"
#include "game_objects/deletion_boxes.hpp"
#include "game_objects/world_obstacles.hpp"
#include "serialization/world_update.hpp"

#include <svanes/kinematic_system.hpp>
#include <svanes/network/message_serialization.hpp>
#include <svanes/sprite_animation_system.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <numbers>
#include <random>
#include <stdexcept>
#include <utility>

constexpr float kPlayerHealth = 100.0F;
constexpr float kPlanetImpactDamage = 25.0F;
constexpr float kCollisionAcceleration = 400000.0F;
constexpr float kSpawnSpeed = 300.0F;

// The number of drifting junk ships.
constexpr std::uint32_t kJunkShipCount = 1200;

// The health of a junk ship.
constexpr float kJunkHealth = 10.0F;

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
    planet.emplace(context.world, Planet::MakeDefinition());
    planet->GetTransform(context.world) = {0.0F, 0.0F};
    repelling_triangle.emplace(CreateRepellingTriangle(context.world));
    rotating_square.emplace(
        CreateRotatingSquare(context.world, gameplay_timeline->Get()));

    deletion_boxes = CreateDeletionBoxes(context.world);
    spawn_zones.emplace(context.world);
    CreateJunk(context.world);
    context.network.Listen("*:" + std::to_string(port));

    std::cout << "Orbital Escalation server: UDP port " << port << '.'
              << std::endl;
}

svanes::JoinResponse OrbitalEscalationServer::HandleJoinRequest(
    const svanes::ServerJoinContext &context) {
    AddPlayer(context.world, context.client);
    std::cout << "Player " << context.client << " joined." << std::endl;
    return {true, {}};
}

void OrbitalEscalationServer::HandleDisconnect(svanes::Registry &,
                                               svanes::ClientId client) {
    players.erase(client);
    std::cout << "Player " << client << " disconnected." << std::endl;
}

OrbitalEscalationServer::Input
OrbitalEscalationServer::DecodeInput(const svanes::NetworkMessage &message) {
    svanes::MessageReader reader(message);
    Input input;

    // An input message consists of the following fields, in order:
    // - 4 bytes: horizontal thrust request, as a 32-bit float
    // - 4 bytes: vertical thrust request, as a 32-bit float
    // - 4 bytes: rotation request, as a 32-bit float
    // - 1 byte: firing request, zero or one
    // - 1 byte: pause toggle request, zero or one

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

    if (!players.empty()) {
        // We need only advance towards sending the next snapshot if there is at
        // least one player to send it to.
        snapshot_tics += svanes::DefaultPhysicsStepTics;
        if (snapshot_tics >= OrbitalSnapshotTics) {
            snapshot_tics %= OrbitalSnapshotTics;
            context.network.Broadcast(CreateWorldUpdate(world));
        }
    }
}

svanes::NetworkMessage OrbitalEscalationServer::CreateWorldUpdate(
    const svanes::Registry &world) const {

    // The encoding of the world update. A self contained message that
    // tells clients everything they need to know about the current state of the
    // game world.
    WorldUpdate update;

    // The master clock shared by all gameplay objects.
    const auto &timeline =
        world.GetComponent<svanes::Timeline>(gameplay_timeline->Get());

    // All gameplay objects follow the master clock, so clients need its time,
    // rate, and pause state along with the objects themselves.
    update.total_tics = timeline.GetTotalTics();
    update.tic_size = timeline.GetTicSize();
    update.paused = timeline.IsPaused();

    // The planet has a shared definition rather than a catalog entry. Only
    // its identity and position need to come from the server (for now...)
    update.planet = planet->GetEntity();
    update.planet_transform =
        world.GetComponent<svanes::Transform>(update.planet);

    update.square_rotation =
        world.GetComponent<svanes::Transform>(rotating_square->Get()).rotation;

    // The list of gameplay objects to include in the update.
    std::vector<const DynamicObject *> pending;

    // First we populate pending with living player ships...
    for (const auto &[client, player] : players) {
        update.players.push_back(
            {client, player.ship ? std::optional{player.ship->GetEntity()}
                                 : std::nullopt});
        if (player.ship) {
            pending.push_back(player.ship.get());
        }
    }

    // ...then all projectiles...
    for (const auto &projectile : projectiles) {
        pending.push_back(projectile.get());
    }

    // ...then the drifting junk ships...
    for (const auto &ship : junk_ships) {
        pending.push_back(ship.get());
    }

    // ...then we visit each object to collect its state. Its attachments are
    // added to pending as we go, so we eventually include the entire tree.
    for (std::size_t index = 0; index < pending.size(); ++index) {
        const auto &object = *pending[index];
        const auto entity = object.GetEntity();

        // The state of this object to include in the world update.
        WorldUpdate::Object state{};

        // The type and definition tell clients what to create. The server's
        // entity ID tells them which existing object this state belongs to.
        state.entity = entity;
        state.type = object.GetType();
        state.definition = object.GetName();

        // Copy its current world pose and elapsed local time.
        state.transform = world.GetComponent<svanes::Transform>(entity);
        state.local_tics =
            world.GetComponent<svanes::Timeline>(entity).GetTotalTics();

        // Attached objects follow their parent and have no independent motion.
        if (world.HasComponent<svanes::Kinematic2D>(entity)) {
            const auto &motion =
                world.GetComponent<svanes::Kinematic2D>(entity);
            state.velocity =
                WorldUpdate::Velocity{{motion.velocity_x, motion.velocity_y},
                                      motion.angular_velocity};
        }

        // Only objects which can receive damage have health to report.
        if (world.HasComponent<Health>(entity)) {
            state.health = world.GetComponent<Health>(entity).remaining;
        }

        // Preserve the parent relationship and relative pose as well as the
        // world pose, so clients can recreate the attachment.
        if (world.HasComponent<Attachment>(entity)) {
            state.attachment = world.GetComponent<Attachment>(entity);
        }

        // Preserve per-object colors, such as the randomly colored junk ships.
        if (world.HasComponent<svanes::Color>(entity)) {
            state.color = world.GetComponent<svanes::Color>(entity);
        }

        update.objects.push_back(std::move(state));

        // Add this object's children so we will visit them as well.
        for (const auto &child : object.GetAttachments()) {
            pending.push_back(child.get());
        }
    }

    return update.Write();
}

void OrbitalEscalationServer::AddPlayer(svanes::Registry &world,
                                        svanes::ClientId client) {
    if (client == 0 || players.contains(client)) {
        throw std::invalid_argument(
            "Orbital player requires a unique, nonzero client ID.");
    }

    auto &player = players.try_emplace(client).first->second;
    SpawnPlayer(world, player);
}

void OrbitalEscalationServer::SpawnPlayer(svanes::Registry &world,
                                          Player &player) {
    // The ships and missiles whose geometry can occupy a spawn zone.
    std::vector<const DynamicObject *> objects;
    for (const auto &[client, other] : players) {
        if (other.ship) {
            objects.push_back(other.ship.get());
        }
    }

    for (const auto &ship : junk_ships) {
        objects.push_back(ship.get());
    }

    for (const auto &projectile : projectiles) {
        objects.push_back(projectile.get());
    }

    const auto position = spawn_zones->ChoosePosition(world, objects);
    player.ship =
        assets.CreateShip(world, gameplay_timeline->Get(), "player_ship");

    player.ship->GetTransform(world) = {position.x, position.y};
    player.ship->GetKinematic(world).velocity_x =
        svanes::PerSecondToPerTic(kSpawnSpeed);
    world.AddComponent<Health>(player.ship->GetEntity(), Health{kPlayerHealth});
    UpdateAttachments(world);
}

void OrbitalEscalationServer::CreateJunk(svanes::Registry &world) {
    std::mt19937 random(std::random_device{}());

    // Random positions around the planet, measured from its surface.
    std::uniform_real_distribution<float> angle_dist(
        0.0F, 2.0F * std::numbers::pi_v<float>);
    std::uniform_real_distribution<float> distance_dist(7500.0F, 70000.0F);

    // The initial tangential speed and spin, in units and radians per second.
    std::uniform_real_distribution<float> velocity_dist(1000.0F, 4200.0F);
    std::uniform_real_distribution<float> spin_dist(-1.0F, 1.0F);
    std::uniform_int_distribution<std::uint16_t> color_dist(0, 255);

    // The three single-shape ship definitions used in turn.
    const std::array names{"junk_square", "junk_circle", "junk_triangle"};

    for (std::uint32_t index = 0; index < kJunkShipCount; ++index) {
        auto ship = assets.CreateShip(world, gameplay_timeline->Get(),
                                      names[index % names.size()]);
        const auto entity = ship->GetEntity();
        const float angle = angle_dist(random);
        const float distance = OrbitalPlanetRadius + distance_dist(random);
        const float speed = svanes::PerSecondToPerTic(velocity_dist(random));

        ship->GetTransform(world) = {std::cos(angle) * distance,
                                     std::sin(angle) * distance};

        // Alternate clockwise and counterclockwise drift around the planet.
        const float direction = index % 2 == 0 ? 1.0F : -1.0F;
        auto &motion = ship->GetKinematic(world);
        motion.velocity_x = -std::sin(angle) * speed * direction;
        motion.velocity_y = std::cos(angle) * speed * direction;
        motion.angular_velocity = svanes::PerSecondToPerTic(spin_dist(random));

        world.AddComponent<Health>(entity, Health{kJunkHealth});
        world.AddComponent<svanes::Color>(
            entity,
            svanes::Color{static_cast<std::uint8_t>(color_dist(random)),
                          static_cast<std::uint8_t>(color_dist(random)),
                          static_cast<std::uint8_t>(color_dist(random)), 255});
        ship->UpdateVisuals(world);
        junk_ships.push_back(std::move(ship));
    }
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

    // The ships contacting the planet in this physics step, including junk.
    std::unordered_set<svanes::Entity> contacting;
    const auto surface = planet->GetEntity();
    for (const auto &pair : contacts) {
        if (pair.a != surface && pair.b != surface) {
            continue;
        }
        const auto entity = pair.a == surface ? pair.b : pair.a;
        if (world.HasComponent<Propulsion>(entity) && !IsDead(world, entity)) {
            // Composite ships can have several contacts with the surface.
            // Deal damage once when contact begins, rather than each tick.
            if (contacting.insert(entity).second &&
                !touching_planet.contains(entity)) {
                ApplyDamage(world, entity, kPlanetImpactDamage);
            }
        }
    }
    touching_planet = std::move(contacting);

    UpdateMissileExplosions(world, contacts);

    // Objects touching the deletion boxes are removed with other dead objects.
    ApplyDeletionBoxes(world, deletion_boxes);

    // Explosions have finished using the entities' geometry. Now their owners
    // can remove dead objects, including dead missiles still inside launchers.
    // A player's roster entry remains so their ship can be respawned.
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

    std::erase_if(junk_ships, [&](const auto &ship) {
        return IsDead(world, ship->GetEntity());
    });

    // Respawn any players whose ships were destroyed.
    for (auto &[client, player] : players) {
        if (!player.ship) {
            SpawnPlayer(world, player);
        }
    }
}
