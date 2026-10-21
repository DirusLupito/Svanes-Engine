#include "world_update.hpp"

#include <stdexcept>
#include <utility>

/**
 * Writes an object's x position, y position, and rotation, in that order,
 * as three consecutive big-endian 32-bit floating-point values.
 *
 * @param writer The message writer to append the pose to.
 * @param pose The object's position and its rotation in radians.
 */
static void WritePose(svanes::MessageWriter &writer,
                      const svanes::Transform &pose) {
    writer.WriteFloat32(pose.x);
    writer.WriteFloat32(pose.y);
    writer.WriteFloat32(pose.rotation);
}

/**
 * Reads an object's x position, y position, and rotation, in that order,
 * from three consecutive big-endian 32-bit floating-point values.
 *
 * @param reader The message reader positioned at the pose to read.
 * @return The decoded position and rotation, with rotation in radians.
 * @throws std::invalid_argument if the message does not contain a complete
 * pose.
 */
static svanes::Transform ReadPose(svanes::MessageReader &reader) {
    return {reader.ReadFloat32(), reader.ReadFloat32(), reader.ReadFloat32()};
}

svanes::NetworkMessage WorldUpdate::Write() const {
    // The writer accumulating the binary world update.
    svanes::MessageWriter writer;

    // First we write the master clock's time, rate, and pause state.

    writer.WriteUint64(total_tics);
    writer.WriteUint64(tic_size.GetNumerator());
    writer.WriteUint64(tic_size.GetDenominator());
    writer.WriteBool(paused);

    // Then we write the planet's identity and pose. Clients create it from the
    // shared definition.
    writer.WriteUint32(planet);
    WritePose(writer, planet_transform);

    // We also write the square's rotation.
    writer.WriteFloat32(square_rotation);

    // Next we send the entire player roster. A player can remain in the game
    // after losing their ship, so ship presence is separate from its entity ID.
    writer.WriteUint32(static_cast<std::uint32_t>(players.size()));
    for (const auto &player : players) {
        writer.WriteUint32(player.client);
        writer.WriteBool(player.ship.has_value());
        if (player.ship) {
            writer.WriteUint32(*player.ship);
        }
    }

    // Finally we write the full object list. Clients can remove objects which
    // appeared in an earlier update but are absent from this one.
    writer.WriteUint32(static_cast<std::uint32_t>(objects.size()));
    for (const auto &object : objects) {
        // Identify the object and its catalog definition before its changing
        // state. Geometry and appearance come from that definition.
        writer.WriteUint32(object.entity);
        writer.WriteUint8(static_cast<std::uint8_t>(object.type));
        writer.WriteText(object.definition);

        // Write its current world pose and elapsed local time.
        WritePose(writer, object.transform);
        writer.WriteUint64(object.local_tics);

        // Write independent movement if present.
        // As with other optional fields, also need to write a presence flag so
        // the reader knows whether to read the next fields as velocity or as
        // something else.
        writer.WriteBool(object.velocity.has_value());
        if (object.velocity) {
            writer.WriteFloat32(object.velocity->linear.x);
            writer.WriteFloat32(object.velocity->linear.y);
            writer.WriteFloat32(object.velocity->angular);
        }

        // Write health and its presence flag.
        writer.WriteBool(object.health.has_value());
        if (object.health) {
            writer.WriteFloat32(*object.health);
        }

        // Write the attachment if present. Children refer to their parent's
        // server entity ID and keep their pose relative to it.
        writer.WriteBool(object.attachment.has_value());
        if (object.attachment) {
            writer.WriteUint32(object.attachment->parent);
            WritePose(writer, object.attachment->transform);
        }

        // Randomly colored ships override the catalog's default appearance.
        writer.WriteBool(object.color.has_value());
        if (object.color) {
            writer.WriteUint8(object.color->red);
            writer.WriteUint8(object.color->green);
            writer.WriteUint8(object.color->blue);
            writer.WriteUint8(object.color->alpha);
        }
    }
    return writer.Finish();
}

WorldUpdate WorldUpdate::Read(const svanes::NetworkMessage &message) {
    // The reader tracking our position in the received message.
    svanes::MessageReader reader(message);

    // The world state described by the received message.
    WorldUpdate update;

    // First read the master clock's time, rate, and pause state
    update.total_tics = reader.ReadUint64();

    const auto numerator = reader.ReadUint64();
    const auto denominator = reader.ReadUint64();

    update.tic_size = {numerator, denominator};
    update.paused = reader.ReadBool();

    // Then read the planet's identity and pose. Its remaining properties
    // come from the shared definition.
    update.planet = reader.ReadUint32();
    update.planet_transform = ReadPose(reader);

    // Restore the square's rotation.
    update.square_rotation = reader.ReadFloat32();

    // The number of players described by this update.
    const auto player_count = reader.ReadUint32();

    // Read all players, including those without a living ship.
    for (std::uint32_t index = 0; index < player_count; ++index) {
        // The player and their optional ship's server entity ID.
        Player player{reader.ReadUint32(), std::nullopt};

        // Entity ID zero is valid, so only the presence flag determines
        // whether this player has a ship.
        if (reader.ReadBool()) {
            player.ship = reader.ReadUint32();
        }

        update.players.push_back(player);
    }

    // The number of gameplay objects described by this update.
    const auto object_count = reader.ReadUint32();

    // Finally read each object's identity, catalog definition, pose, local
    // time, and optional movement, health, attachment, and color.
    for (std::uint32_t index = 0; index < object_count; ++index) {
        // The state of the object currently being read.
        Object object{};
        object.entity = reader.ReadUint32();

        // The kind of catalog definition used to create this object.
        const auto type = reader.ReadUint8();

        // Reject types which do not describe an Orbital Escalation object.
        if (type >= static_cast<std::uint8_t>(DynamicObjectType::Count)) {
            throw std::invalid_argument(
                "Unknown Orbital Escalation object type.");
        }
        object.type = static_cast<DynamicObjectType>(type);
        object.definition = reader.ReadText();

        // Read its current world pose and elapsed local time.
        object.transform = ReadPose(reader);
        object.local_tics = reader.ReadUint64();

        // Read independent movement if present. Each presence flag determines
        // whether the optional state follows or the next field starts instead.
        if (reader.ReadBool()) {
            object.velocity =
                Velocity{{reader.ReadFloat32(), reader.ReadFloat32()},
                         reader.ReadFloat32()};
        }

        // Read health if present.
        if (reader.ReadBool()) {
            object.health = reader.ReadFloat32();
        }

        // we keep the server's parent ID because it's one source of truth
        // for everyone to agree upon. Acting upon something will require the
        // client to tell everyone else what it did, and the server only
        // understands the server's entity IDs.

        if (reader.ReadBool()) {
            // The server entity ID of the object this one is attached to.
            const auto parent = reader.ReadUint32();

            object.attachment = Attachment{parent, ReadPose(reader)};
        }

        // An optional color overrides the object's solid visual parts.
        if (reader.ReadBool()) {
            object.color =
                svanes::Color{reader.ReadUint8(), reader.ReadUint8(),
                              reader.ReadUint8(), reader.ReadUint8()};
        }
        update.objects.push_back(std::move(object));
    }

    // All bytes should belong to the state above. Leftover data means the
    // message does not match the world update format we just read.
    if (reader.Remaining() != 0) {
        throw std::invalid_argument(
            "Trailing data in Orbital Escalation world update.");
    }
    return update;
}
