#include "client_event_handlers.hpp"

#include "combat/damage_system.hpp"
#include "combat/missile_system.hpp"
#include "combat/weapon_system.hpp"
#include "effects/collision_flashes.hpp"
#include "player_events.hpp"
#include "world_snapshot_event.hpp"

#include <svanes/MenuUtilities/text_label.hpp>
#include <svanes/camera2d.hpp>
#include <svanes/events/event_manager.hpp>
#include <svanes/registry.hpp>

#include <algorithm>
#include <stdexcept>
#include <unordered_set>
#include <utility>

ClientEventHandlers::ClientEventHandlers(ClientEventContext context)
    : context(context) {}

void ClientEventHandlers::HandleWorldSnapshot(const WorldSnapshotEvent &event) {
    snapshot = event.snapshot;
    auto &world = context.world;
    auto &objects = context.objects;
    auto &root_objects = context.root_objects;

    auto &timeline =
        world.GetComponent<svanes::Timeline>(context.gameplay_timeline);
    auto &effects =
        world.GetComponent<svanes::Timeline>(context.effects_timeline);
    timeline.SetProgress(snapshot.total_tics, 0, svanes::RationalNumber{0});
    timeline.SetTicSize(snapshot.tic_size);
    if (snapshot.paused) {
        timeline.Pause();
        effects.Pause();
    } else {
        timeline.Unpause();
        effects.Unpause();
    }
    world.GetComponent<svanes::TextLabel>(context.pause_label).visible =
        snapshot.paused;

    if (!context.planet) {
        context.planet.emplace(world, Planet::MakeDefinition());
    }
    context.planet->GetTransform(world) = snapshot.planet_transform;
    context.planet->UpdateVisuals(world);
    world.GetComponent<svanes::Transform>(context.rotating_square).rotation =
        snapshot.square_rotation;

    // A ship can die after firing a missile which still exists in the snapshot.
    // Separate attachments before removing ships so those objects can survive.
    for (std::size_t index = 0; index < root_objects.size(); ++index) {
        auto attachments = root_objects[index]->ReleaseAttachments();
        world.RemoveComponent<Attachment>(root_objects[index]->GetEntity());
        for (auto &attachment : attachments) {
            root_objects.push_back(std::move(attachment));
        }
    }

    std::vector<std::uint32_t> dead_players;
    for (const auto &[player, ship] : player_ships) {
        const auto current = std::find_if(
            snapshot.players.begin(), snapshot.players.end(),
            [player](const auto &entry) { return entry.client == player; });
        if (current == snapshot.players.end() || current->ship != ship) {
            dead_players.push_back(player);
        }
    }
    for (const auto player : dead_players) {
        context.events.TriggerEvent(PlayerDeathEvent{player});
        if (player_ships.contains(player)) {
            throw std::logic_error("The player death handler must be "
                                   "registered before applying snapshots.");
        }
    }
    for (const auto &player : snapshot.players) {
        if (player.ship && !player_ships.contains(player.client)) {
            // The ship must exist before its attachments can be restored.
            context.events.TriggerEvent(PlayerSpawnEvent{player.client});
            if (!player_ships.contains(player.client)) {
                throw std::logic_error("The player spawn handler must be "
                                       "registered before applying snapshots.");
            }
        }
    }

    // Retain ownership while rebuilding trees. Predicted-only and absent
    // objects stay in this map and are destroyed when the snapshot is applied.
    std::unordered_map<svanes::Entity, std::unique_ptr<DynamicObject>> owned;
    for (auto &object : root_objects) {
        owned.emplace(object->GetEntity(), std::move(object));
    }
    root_objects.clear();

    std::unordered_set<svanes::Entity> present;
    for (const auto &state : snapshot.objects) {
        present.insert(state.entity);
        if (!objects.contains(state.entity)) {
            auto object = context.assets.CreateUnattached(
                world, context.gameplay_timeline, state.type, state.definition);
            objects.emplace(state.entity, object.get());
            owned.emplace(object->GetEntity(), std::move(object));
        }
        ApplyObjectState(state);
    }

    // Missiles absent from a complete snapshot are assumed to have detonated.
    for (const auto &[id, object] : objects) {
        if (!present.contains(id) &&
            object->GetType() == DynamicObjectType::Missile) {
            const auto &pose = object->GetTransform(world);
            CreateCollisionFlash(world, context.effects_timeline,
                                 context.collision_flashes, {pose.x, pose.y});
        }
    }
    std::erase_if(objects, [&](const auto &entry) {
        return !present.contains(entry.first);
    });

    for (const auto &state : snapshot.objects) {
        auto &object = *objects.at(state.entity);
        const auto entity = object.GetEntity();
        if (state.attachment) {
            objects.at(state.attachment->parent)
                ->AddAttachment(world, std::move(owned.at(entity)),
                                state.attachment->transform);
        } else {
            root_objects.push_back(std::move(owned.at(entity)));
        }
    }

    // Attaching objects recalculates poses. Restore the exact server poses
    // after the complete attachment trees exist.
    for (const auto &state : snapshot.objects) {
        objects.at(state.entity)->GetTransform(world) = state.transform;
    }

    // Estimate launcher readiness from the ammunition still attached to it.
    // Empty slots start a new local reload countdown.
    for (const auto &state : snapshot.objects) {
        const auto &object = *objects.at(state.entity);
        const auto entity = object.GetEntity();
        if (world.HasComponent<MissileLauncherState>(entity)) {
            auto &launcher = world.GetComponent<MissileLauncherState>(entity);
            launcher.loaded_missile.reset();
            launcher.emptied_at = state.local_tics;
            launcher.ammunition_lost = false;
            for (const auto &child : object.GetAttachments()) {
                if (child->GetType() == DynamicObjectType::Missile) {
                    launcher.loaded_missile = child->GetEntity();
                    launcher.emptied_at.reset();
                    break;
                }
            }
            world.GetComponent<WeaponControl>(entity).fire = false;
        }
    }
    for (const auto &object : root_objects) {
        object->UpdateVisuals(world);
    }

    if (context.player_ship) {
        const auto &pose =
            objects.at(*context.player_ship)->GetTransform(world);
        context.camera.x = pose.x;
        context.camera.y = pose.y;
    }
}

void ClientEventHandlers::HandlePlayerSpawn(const PlayerSpawnEvent &event) {
    const auto player = std::find_if(
        snapshot.players.begin(), snapshot.players.end(),
        [&event](const auto &entry) { return entry.client == event.player; });
    if (player == snapshot.players.end() || !player->ship) {
        throw std::invalid_argument(
            "The snapshot has no ship for the spawning player.");
    }
    const auto state =
        std::find_if(snapshot.objects.begin(), snapshot.objects.end(),
                     [&player](const auto &object) {
                         return object.entity == *player->ship;
                     });
    if (state == snapshot.objects.end() ||
        state->type != DynamicObjectType::Ship || state->attachment) {
        throw std::invalid_argument(
            "A player must spawn as an independent ship in the snapshot.");
    }
    if (player_ships.contains(event.player) ||
        context.objects.contains(state->entity)) {
        throw std::logic_error("The player or their ship already exists.");
    }

    auto ship = context.assets.CreateUnattached(context.world,
                                                context.gameplay_timeline,
                                                state->type, state->definition);
    ship->GetTransform(context.world) = state->transform;
    context.objects.emplace(state->entity, ship.get());
    context.root_objects.push_back(std::move(ship));
    ApplyObjectState(*state);
    context.objects.at(state->entity)->UpdateVisuals(context.world);
    player_ships.emplace(event.player, state->entity);
    if (event.player == context.client_id) {
        context.player_ship = state->entity;
    }
}

void ClientEventHandlers::HandlePlayerDeath(const PlayerDeathEvent &event) {
    const auto ship =
        context.objects.at(player_ships.at(event.player))->GetEntity();
    std::erase_if(context.objects, [&](const auto &entry) {
        return GetAttachmentRoot(context.world, entry.second->GetEntity()) ==
               ship;
    });
    std::erase_if(context.root_objects, [ship](const auto &object) {
        return object->GetEntity() == ship;
    });
    player_ships.erase(event.player);
    if (event.player == context.client_id) {
        context.player_ship.reset();
    }
}

bool ClientEventHandlers::IsPlayerShip(svanes::Entity entity) const {
    return std::any_of(
        player_ships.begin(), player_ships.end(), [&](const auto &player) {
            return context.objects.at(player.second)->GetEntity() == entity;
        });
}

void ClientEventHandlers::ApplyObjectState(const WorldUpdate::Object &state) {
    auto &world = context.world;
    const auto entity = context.objects.at(state.entity)->GetEntity();
    world.GetComponent<svanes::Timeline>(entity).SetProgress(
        state.local_tics, 0, svanes::RationalNumber{0});
    if (state.velocity) {
        auto &motion = world.AddComponent<svanes::Kinematic2D>(entity);
        motion.velocity_x = state.velocity->linear.x;
        motion.velocity_y = state.velocity->linear.y;
        motion.angular_velocity = state.velocity->angular;
    } else {
        world.RemoveComponent<svanes::Kinematic2D>(entity);
    }
    if (state.health) {
        world.AddComponent<Health>(entity, Health{*state.health});
    } else {
        world.RemoveComponent<Health>(entity);
    }
    if (state.color) {
        world.AddComponent<svanes::Color>(entity, *state.color);
    } else {
        world.RemoveComponent<svanes::Color>(entity);
    }

    // Launch history is estimated locally rather than sent by the server.
    if (world.HasComponent<MissileExplosion>(entity)) {
        auto &explosion = world.GetComponent<MissileExplosion>(entity);
        explosion.firing_ship.reset();
        explosion.launched_at = state.local_tics;
        explosion.detonated = false;
    }
}
