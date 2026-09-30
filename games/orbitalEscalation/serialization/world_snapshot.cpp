#include "world_snapshot.hpp"
#include "../attachment_system.hpp"
#include "../orbital_simulation.hpp"
#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>
#include <svanes/stable_id.hpp>

using svanes::MessageReader;
using svanes::MessageWriter;

/**
 * Writes an optional value, preserving whether it was present in the saved
 * state.
 *
 * @tparam T The type of value to save.
 * @tparam Write The callable type used to write a present value.
 *
 * @param writer The message to append to.
 * @param value The optional value to save.
 * @param write Writes a present value to writer. Its exceptions propagate.
 */
template <typename T, typename Write>
static void WriteOptional(MessageWriter &writer, const std::optional<T> &value,
                          Write write) {
    writer.WriteBool(value.has_value());
    if (value) {
        write(writer, *value);
    }
}

/**
 * Reads a saved optional value, returning no value when it was absent.
 *
 * @tparam Read The callable type used to read a present value.
 *
 * @param reader The message positioned at a saved optional value.
 * @param read Reads a present value from reader. Its exceptions propagate.
 *
 * @return The saved value, or std::nullopt if it was absent.
 *
 * @throws std::invalid_argument if the presence flag is truncated or invalid.
 */
template <typename Read>
static auto ReadOptional(MessageReader &reader, Read read)
    -> std::optional<decltype(read(reader))> {
    if (reader.ReadBool()) {
        return read(reader);
    }
    return std::nullopt;
}

/**
 * Writes a collection size that the snapshot format can represent.
 *
 * @param writer The message to append to.
 * @param count The number of entries in the collection.
 *
 * @throws std::length_error if the count exceeds the format's limit.
 */
static void WriteCount(MessageWriter &writer, std::size_t count) {
    if (count > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("Orbital snapshot count exceeds uint32_t");
    }
    writer.WriteUint32(static_cast<std::uint32_t>(count));
}

/**
 * Reads a collection size and rejects counts that cannot fit in the
 * remaining message.
 *
 * @param reader The message positioned at a count written by WriteCount.
 * @param minimum_size The minimum number of bytes needed for one entry.
 * Must be greater than zero.
 *
 * @return The number of entries to read.
 *
 * @throws std::invalid_argument if the count is truncated or the entries
 * cannot fit in the remaining message.
 */
static std::uint32_t ReadCount(MessageReader &reader,
                               std::size_t minimum_size) {
    const auto count = reader.ReadUint32();
    if (count > reader.Remaining() / minimum_size) {
        throw std::invalid_argument(
            "Orbital snapshot count exceeds remaining data");
    }
    return count;
}

/**
 * Saves an ordered list of shared identities so ownership and simulation
 * order can be restored.
 *
 * @param writer The message to append to.
 * @param ids The shared identities in the order to preserve.
 *
 * @throws std::length_error if the number of identities exceeds the
 * snapshot format's limit.
 */
static void WriteIds(MessageWriter &writer,
                     const std::vector<std::uint64_t> &ids) {
    WriteCount(writer, ids.size());
    for (auto id : ids) {
        writer.WriteUint64(id);
    }
}

/**
 * Reads the saved shared identities in their original order.
 *
 * @param reader The message positioned at identities written by WriteIds.
 *
 * @return The saved shared identities, in order.
 *
 * @throws std::invalid_argument if the list is truncated or its count
 * cannot fit in the remaining message.
 */
static std::vector<std::uint64_t> ReadIds(MessageReader &reader) {
    const auto count = ReadCount(reader, 8);
    std::vector<std::uint64_t> ids;
    ids.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        ids.push_back(reader.ReadUint64());
    }
    return ids;
}

/**
 * Saves a clock and its parent so gameplay timing can be restored.
 *
 * @param writer The message to append to.
 * @param clock The clock state and shared parent to save.
 */
static void WriteClock(MessageWriter &writer, const SnapshotTimeline &clock) {
    writer.WriteUint64(clock.parent);
    svanes::WriteTimeline(writer, clock.value);
}

/**
 * Reads the saved clock state and shared parent for restoration.
 *
 * @param reader The message positioned at a clock written by WriteClock.
 *
 * @return The saved clock state and shared parent.
 *
 * @throws std::invalid_argument if the clock data is truncated or invalid.
 */
static SnapshotTimeline ReadClock(MessageReader &reader) {
    const auto parent = reader.ReadUint64();
    return {svanes::ReadTimeline(reader), parent};
}

/**
 * Saves an exact tic count so gameplay timers can resume at the same point.
 *
 * @param writer The message to append to.
 * @param value The exact tic count to save.
 */
static void WriteTics(MessageWriter &writer, svanes::TicCount value) {
    writer.WriteUint64(value);
}

/**
 * Reads an exact saved tic count for restoring gameplay timers.
 *
 * @param reader The message positioned at a tic count written by WriteTics.
 *
 * @return The saved tic count.
 *
 * @throws std::invalid_argument if the tic count is truncated.
 */
static svanes::TicCount ReadTics(MessageReader &reader) {
    return reader.ReadUint64();
}

/**
 * Saves an object's mutable gameplay state for later restoration.
 *
 * @param writer The message to append to.
 * @param body The saved gameplay components and shared references to write.
 */
static void WriteBody(MessageWriter &writer, const SnapshotBody &body) {
    svanes::WriteTransform(writer, body.transform);
    WriteOptional(writer, body.motion, svanes::WriteKinematic);
    WriteOptional(writer, body.timeline, WriteClock);
    WriteOptional(writer, body.health, [](auto &out, const Health &health) {
        out.WriteFloat32(health.remaining);
    });
    WriteOptional(writer, body.propulsion_control,
                  [](auto &out, const PropulsionControl &control) {
                      out.WriteFloat32(control.thrust.x);
                      out.WriteFloat32(control.thrust.y);
                      out.WriteFloat32(control.rotation);
                  });
    WriteOptional(writer, body.weapon_control,
                  [](auto &out, const WeaponControl &control) {
                      out.WriteBool(control.fire);
                  });
    WriteOptional(writer, body.explosion,
                  [&](auto &out, const MissileExplosion &explosion) {
                      out.WriteUint64(body.firing_ship);
                      out.WriteUint64(explosion.launched_at);
                      out.WriteBool(explosion.detonated);
                  });
    WriteOptional(writer, body.launcher,
                  [&](auto &out, const MissileLauncherState &launcher) {
                      out.WriteUint64(body.loaded_missile);
                      WriteOptional(out, launcher.emptied_at, WriteTics);
                      out.WriteBool(launcher.ammunition_lost);
                  });
    WriteOptional(writer, body.attachment,
                  [](auto &out, const SnapshotAttachment &attachment) {
                      out.WriteUint64(attachment.parent);
                      svanes::WriteTransform(out, attachment.transform);
                  });
}

/**
 * Reads the saved gameplay state to apply when recreating an object.
 *
 * For example, an attached missile could have the following saved values,
 * in this order,
 *
 *     transform:          (x=10, y=20, rotation=0)
 *     motion:             false
 *     timeline:           true, parent=1, tic_size=(1, 1), paused=false,
 *                         total=20000, delta=10000, incomplete=(0, 1)
 *     health:             true, remaining=10
 *     propulsion_control: false
 *     weapon_control:     false
 *     explosion:          true, firing_ship=0, launched_at=0, detonated=false
 *     launcher:           false
 *     attachment:         true, parent=7, transform=(x=0, y=-20, rotation=0)
 *
 * The transform is always present. Each subsequent component starts with a
 * one-byte presence flag. 0 for false, 1 for true. If false, there is no
 * component data after that flag and we immediately read the next component's
 * flag. For example, motion=false above means there are no saved velocities
 * or accelerations between the motion flag and the timeline flag.
 *
 * @param reader The message positioned at a body written by WriteBody.
 *
 * @return The saved gameplay components and shared references.
 *
 * @throws std::invalid_argument if the data is truncated, a boolean flag
 * is invalid, or a saved clock is invalid.
 */
static SnapshotBody ReadBody(MessageReader &reader) {
    SnapshotBody body;
    body.transform = svanes::ReadTransform(reader);
    body.motion = ReadOptional(reader, svanes::ReadKinematic);
    body.timeline = ReadOptional(reader, ReadClock);

    body.health =
        ReadOptional(reader, [](auto &in) { return Health{in.ReadFloat32()}; });

    body.propulsion_control = ReadOptional(reader, [](auto &in) {
        return PropulsionControl{{in.ReadFloat32(), in.ReadFloat32()},
                                 in.ReadFloat32()};
    });

    body.weapon_control = ReadOptional(
        reader, [](auto &in) { return WeaponControl{in.ReadBool()}; });
    body.explosion = ReadOptional(reader, [&](auto &in) {
        MissileExplosion explosion{};
        body.firing_ship = in.ReadUint64();
        explosion.launched_at = in.ReadUint64();
        explosion.detonated = in.ReadBool();
        return explosion;
    });

    body.launcher = ReadOptional(reader, [&](auto &in) {
        MissileLauncherState launcher{};
        body.loaded_missile = in.ReadUint64();
        launcher.emptied_at = ReadOptional(in, ReadTics);
        launcher.ammunition_lost = in.ReadBool();
        return launcher;
    });

    body.attachment = ReadOptional(reader, [](auto &in) {
        const auto parent = in.ReadUint64();
        return SnapshotAttachment{parent, svanes::ReadTransform(in)};
    });

    return body;
}


// Simplified example of what a WorldSnapshot might look like when serialized.
// world with one player's ship, one launcher attached to that
// ship, and one missile that has already been fired:
//
//     next_identity:   6
//     root_identity:   1
//     root:            parent=0, tic_size=(1, 1), paused=false,
//                      total=20000, delta=10000, incomplete=(0, 1)
//     planet_identity: 2
//     planet:          <planet body>
//     player_count:    1
//         peer=1, ship=3, touching_planet=false
//     object_count:    3
//         identity=3, type=0, name="player_ship", <ship body>,
//             child_count=1, children=[4]
//         identity=4, type=2, name="basic_missile_single_launcher",
//             <launcher body>, child_count=0
//         identity=5, type=1, name="basic_missile", <missile body>,
//             child_count=0
//     projectile_count: 1
//         projectiles=[5]

svanes::NetworkMessage WriteWorldSnapshot(const WorldSnapshot &snapshot) {
    MessageWriter writer;
    writer.WriteUint64(snapshot.next_identity);
    writer.WriteUint64(snapshot.root_identity);
    WriteClock(writer, snapshot.root);
    writer.WriteUint64(snapshot.planet_identity);
    WriteBody(writer, snapshot.planet);
    WriteCount(writer, snapshot.players.size());
    for (const auto &player : snapshot.players) {
        writer.WriteUint32(player.peer);
        writer.WriteUint64(player.ship);
        writer.WriteBool(player.touching_planet);
    }

    WriteCount(writer, snapshot.objects.size());
    for (const auto &object : snapshot.objects) {
        writer.WriteUint64(object.identity);
        writer.WriteUint8(static_cast<std::uint8_t>(object.type));
        writer.WriteText(object.name);
        WriteBody(writer, object.body);
        WriteIds(writer, object.children);
    }

    WriteIds(writer, snapshot.projectiles);
    return writer.Finish();
}

WorldSnapshot ReadWorldSnapshot(std::span<const std::byte> bytes) {
    MessageReader reader(bytes);
    WorldSnapshot snapshot;
    snapshot.next_identity = reader.ReadUint64();
    snapshot.root_identity = reader.ReadUint64();
    snapshot.root = ReadClock(reader);
    snapshot.planet_identity = reader.ReadUint64();
    snapshot.planet = ReadBody(reader);
    const auto players = ReadCount(reader, 13);
    for (std::uint32_t i = 0; i < players; ++i) {
        snapshot.players.push_back(
            {reader.ReadUint32(), reader.ReadUint64(), reader.ReadBool()});
    }

    const auto objects = ReadCount(reader, 23);
    for (std::uint32_t i = 0; i < objects; ++i) {
        SnapshotObject object;
        object.identity = reader.ReadUint64();
        const auto type = reader.ReadUint8();
        if (type >
            static_cast<std::uint8_t>(DynamicObjectType::MissileLauncher)) {
            throw std::invalid_argument("Unknown Orbital snapshot object type");
        }

        object.type = static_cast<DynamicObjectType>(type);
        object.name = reader.ReadText();
        object.body = ReadBody(reader);
        object.children = ReadIds(reader);
        snapshot.objects.push_back(std::move(object));
    }

    snapshot.projectiles = ReadIds(reader);
    if (reader.Remaining() != 0) {
        throw std::invalid_argument("Trailing Orbital snapshot data");
    }

    return snapshot;
}

/**
 * Captures a component's state, including its absence, for restoration.
 *
 * @tparam T The component type to capture.
 * @param world The registry containing the entity.
 * @param entity The entity whose component to save.
 * @return A copy of the component, or std::nullopt if it is absent.
 */
template <typename T>
static std::optional<T> CaptureComponent(const svanes::Registry &world,
                                         svanes::Entity entity) {
    if (world.HasComponent<T>(entity)) {
        return world.GetComponent<T>(entity);
    }

    return std::nullopt;
}

/**
 * Gets the identity peers use to refer to the same gameplay entity.
 *
 * @param world The registry containing the entity.
 * @param entity The gameplay entity whose shared identity to get.
 * @return The entity's shared identity.
 * @throws std::invalid_argument if the entity has no shared identity.
 */
static std::uint64_t SharedIdentity(const svanes::Registry &world,
                                    svanes::Entity entity) {
    if (!world.HasComponent<svanes::StableId>(entity)) {
        throw std::invalid_argument(
            "Orbital gameplay reference has no shared identity");
    }

    return world.GetComponent<svanes::StableId>(entity).value;
}

/**
 * Captures a clock's progress and parent so restoration preserves its
 * place in gameplay time.
 *
 * @param world The registry containing the clock.
 * @param entity The entity whose timeline to save.
 * @return The clock state and its parent's shared identity.
 * @throws std::out_of_range if the entity has no timeline.
 * @throws std::invalid_argument if the parent has no shared identity.
 */
static SnapshotTimeline CaptureClock(const svanes::Registry &world,
                                     svanes::Entity entity) {
    const auto &timeline = world.GetComponent<svanes::Timeline>(entity);
    return {timeline, timeline.GetParent()
                          ? SharedIdentity(world, *timeline.GetParent())
                          : 0};
}

/**
 * Collects an entity's mutable gameplay state into a snapshot for restoration.
 *
 * @param world The registry containing the entity.
 * @param entity The entity whose gameplay state to save.
 * @return The saved gameplay components and shared references.
 * @throws std::out_of_range if the entity has no transform.
 * @throws std::invalid_argument if a required gameplay reference has no
 * shared identity.
 */
static SnapshotBody CaptureBody(const svanes::Registry &world,
                                svanes::Entity entity) {
    SnapshotBody body;
    body.transform = world.GetComponent<svanes::Transform>(entity);
    body.motion = CaptureComponent<svanes::Kinematic2D>(world, entity);
    if (world.HasComponent<svanes::Timeline>(entity)) {
        body.timeline = CaptureClock(world, entity);
    }
    body.health = CaptureComponent<Health>(world, entity);
    body.propulsion_control =
        CaptureComponent<PropulsionControl>(world, entity);
    body.weapon_control = CaptureComponent<WeaponControl>(world, entity);
    body.explosion = CaptureComponent<MissileExplosion>(world, entity);
    if (body.explosion && body.explosion->firing_ship &&
        world.HasComponent<svanes::StableId>(*body.explosion->firing_ship)) {
        body.firing_ship = SharedIdentity(world, *body.explosion->firing_ship);
    }
    body.launcher = CaptureComponent<MissileLauncherState>(world, entity);
    if (body.launcher && body.launcher->loaded_missile) {
        const auto ammo = *body.launcher->loaded_missile;
        if (world.HasComponent<svanes::StableId>(ammo)) {
            body.loaded_missile = SharedIdentity(world, ammo);
        } else {
            // Remember the destroyed missile so restoration preserves when
            // the launcher starts reloading.
            body.launcher->ammunition_lost = true;
        }
    }
    if (world.HasComponent<Attachment>(entity)) {
        const auto &attachment = world.GetComponent<Attachment>(entity);
        body.attachment = {SharedIdentity(world, attachment.parent),
                           attachment.transform};
    }
    return body;
}

svanes::NetworkMessage OrbitalSimulation::Save() const {
    WorldSnapshot snapshot;
    snapshot.next_identity = next_identity;
    snapshot.root_identity = SharedIdentity(world, gameplay_timeline.Get());
    snapshot.root = CaptureClock(world, gameplay_timeline.Get());
    snapshot.planet_identity = SharedIdentity(world, planet->GetEntity());
    snapshot.planet = CaptureBody(world, planet->GetEntity());

    // Holds a list of objects to serialize, including attachments.
    std::vector<const DynamicObject *> pending;

    // First we add the players and all projectiles to the pending list.
    for (const auto &[peer, player] : players) {
        snapshot.players.push_back(
            {peer,
             player.ship ? SharedIdentity(world, player.ship->GetEntity()) : 0,
             player.touching_planet});
        if (player.ship) {
            pending.push_back(player.ship.get());
        }
    }

    for (const auto &projectile : projectiles) {
        snapshot.projectiles.push_back(
            SharedIdentity(world, projectile->GetEntity()));
        pending.push_back(projectile.get());
    }

    // Players and projectiles can have attachments. Attachments form a forest
    // of disjoint trees, where each child node has a unique parent, so we know
    // there are no duplicates in the pending list.
    for (std::size_t i = 0; i < pending.size(); ++i) {
        const auto &object = *pending[i];
        SnapshotObject saved{SharedIdentity(world, object.GetEntity()),
                             object.GetType(), object.GetName(),
                             CaptureBody(world, object.GetEntity())};
        for (const auto &child : object.GetAttachments()) {
            saved.children.push_back(SharedIdentity(world, child->GetEntity()));
            pending.push_back(child.get());
        }
        snapshot.objects.push_back(std::move(saved));
    }
    // Sort by shared identity so peers saving the same world produce the same
    // bytes for state comparisons.
    std::sort(
        snapshot.objects.begin(), snapshot.objects.end(),
        [](const auto &a, const auto &b) { return a.identity < b.identity; });
    return WriteWorldSnapshot(snapshot);
}

/**
 * Restores a component to its saved state, including removing it if it was
 * absent.
 *
 * @tparam T The component type to restore.
 * @param world The registry containing the entity.
 * @param entity The entity whose component to restore.
 * @param value The saved component, or std::nullopt to remove it.
 */
template <typename T>
static void RestoreComponent(svanes::Registry &world, svanes::Entity entity,
                             const std::optional<T> &value) {
    if (value) {
        world.AddComponent<T>(entity, *value);
    } else {
        world.RemoveComponent<T>(entity);
    }
}

/**
 * Recreates a saved clock with its parent in the restored world, preserving
 * its progress.
 *
 * @param clock The saved clock state and shared parent.
 * @param entities The mapping from shared identities to restored entities.
 * @return The clock with its saved progress and restored parent.
 * @throws std::out_of_range if the saved parent is missing from entities.
 */
static svanes::Timeline
RestoreClock(const SnapshotTimeline &clock,
             const std::map<std::uint64_t, svanes::Entity> &entities) {
    auto value = clock.value;
    value.SetParent(clock.parent ? std::optional{entities.at(clock.parent)}
                                 : std::nullopt);
    value.SetProgress(clock.value.GetTotalTics(), clock.value.GetDeltaTics(),
                      clock.value.GetIncompleteProgress());
    return value;
}

/**
 * Applies saved gameplay state to an entity recreated from its definition.
 *
 * @param world The registry containing the recreated entity.
 * @param entity The entity created from its matching definition.
 * @param body The saved gameplay state to apply.
 * @param entities The mapping from shared identities to restored entities.
 * @throws std::out_of_range if a saved reference is missing from entities
 * or the entity lacks a required weapon component.
 */
static void
RestoreBody(svanes::Registry &world, svanes::Entity entity,
            const SnapshotBody &body,
            const std::map<std::uint64_t, svanes::Entity> &entities) {
    world.AddComponent<svanes::Transform>(entity, body.transform);
    RestoreComponent(world, entity, body.motion);
    RestoreComponent(world, entity, body.health);
    RestoreComponent(world, entity, body.propulsion_control);
    RestoreComponent(world, entity, body.weapon_control);

    if (body.timeline) {
        world.AddComponent<svanes::Timeline>(
            entity, RestoreClock(*body.timeline, entities));
    } else {
        world.RemoveComponent<svanes::Timeline>(entity);
    }

    // Weapons keep their fixed properties from the definitions. Only their
    // progress during gameplay needs to be restored.
    if (body.explosion) {
        auto &explosion = world.GetComponent<MissileExplosion>(entity);
        explosion.firing_ship =
            body.firing_ship ? std::optional{entities.at(body.firing_ship)}
                             : std::nullopt;
        explosion.launched_at = body.explosion->launched_at;
        explosion.detonated = body.explosion->detonated;
    }

    if (body.launcher) {
        auto &launcher = world.GetComponent<MissileLauncherState>(entity);
        launcher.loaded_missile =
            body.loaded_missile
                ? std::optional{entities.at(body.loaded_missile)}
                : std::nullopt;
        launcher.emptied_at = body.launcher->emptied_at;
        launcher.ammunition_lost = body.launcher->ammunition_lost;
    }

    if (body.attachment) {
        world.AddComponent<Attachment>(
            entity, Attachment{entities.at(body.attachment->parent),
                               body.attachment->transform});
    }
}

void OrbitalSimulation::Load(std::span<const std::byte> bytes) {
    const auto snapshot = ReadWorldSnapshot(bytes);
    // Keep the old world until its replacement is ready. If reconstruction
    // fails, the temporary owners clean up the new entities.
    OwnedEntity replacement_timeline(world);
    std::optional<Planet> replacement_planet(std::in_place, world,
                                             MakePlanetDefinition());

    std::map<std::uint64_t, svanes::Entity> entities{
        {snapshot.root_identity, replacement_timeline.Get()},
        {snapshot.planet_identity, replacement_planet->GetEntity()}};

    // Create all objects first so references can point to objects later in
    // the snapshot.
    std::map<std::uint64_t, std::unique_ptr<DynamicObject>> objects;
    std::map<std::uint64_t, const SnapshotObject *> records;

    for (const auto &record : snapshot.objects) {
        auto object = assets.CreateUnattached(world, replacement_timeline.Get(),
                                              record.type, record.name);
        entities.emplace(record.identity, object->GetEntity());
        records.emplace(record.identity, &record);
        objects.emplace(record.identity, std::move(object));
    }

    world.AddComponent<svanes::Timeline>(replacement_timeline.Get(),
                                         RestoreClock(snapshot.root, entities));

    RestoreBody(world, replacement_planet->GetEntity(), snapshot.planet,
                entities);
    for (const auto &record : snapshot.objects) {
        RestoreBody(world, entities.at(record.identity), record.body, entities);
    }
    for (const auto &[id, entity] : entities) {
        world.AddComponent<svanes::StableId>(entity, svanes::StableId{id});
    }

    // An ordered list of all objects, including attachments, so we can assemble
    // the attachment hierarchy. The order is important because each child must
    // be attached before its parent, and the order of siblings affects
    // gameplay.
    std::vector<std::uint64_t> order;
    for (const auto &player : snapshot.players) {
        if (player.ship) {
            order.push_back(player.ship);
        }
    }

    order.insert(order.end(), snapshot.projectiles.begin(),
                 snapshot.projectiles.end());
    std::set<std::uint64_t> owned;

    for (std::size_t i = 0; i < order.size(); ++i) {
        if (!owned.insert(order[i]).second) {
            throw std::invalid_argument(
                "Orbital snapshot object has more than one owner");
        }
        const auto &children = records.at(order[i])->children;
        order.insert(order.end(), children.begin(), children.end());
    }

    if (owned.size() != objects.size()) {
        throw std::invalid_argument(
            "Orbital snapshot contains unowned objects");
    }

    // Assemble children attachments before their parents so each child brings
    // its descendants with it.
    for (auto it = order.rbegin(); it != order.rend(); ++it) {
        auto &object = objects.at(*it);
        const auto &children = records.at(*it)->children;
        object->attachments.reserve(children.size());
        for (auto child : children) {
            object->attachments.push_back(std::move(objects.at(child)));
        }
    }

    std::map<std::uint32_t, Player> replacement_players;
    for (const auto &player : snapshot.players) {
        replacement_players.emplace(
            player.peer,
            Player{player.ship ? std::move(objects.at(player.ship)) : nullptr,
                   player.touching_planet});
    }

    std::vector<std::unique_ptr<DynamicObject>> replacement_projectiles;
    replacement_projectiles.reserve(snapshot.projectiles.size());
    for (auto id : snapshot.projectiles) {
        replacement_projectiles.push_back(std::move(objects.at(id)));
    }

    collision_flashes.clear();
    players.swap(replacement_players);
    projectiles.swap(replacement_projectiles);
    planet.swap(replacement_planet);
    std::swap(gameplay_timeline, replacement_timeline);
    next_identity = snapshot.next_identity;
    UpdateVisuals();
}
