#include "protocol.hpp"

#include <svanes/network/message_serialization.hpp>

#include <stdexcept>

namespace {

/**
 * @param writer The message to append to.
 * @param position The position to write.
 */
void WritePosition(svanes::MessageWriter &writer, svanes::Vector2D position) {
    writer.WriteFloat32(position.x);
    writer.WriteFloat32(position.y);
}

/**
 * @param reader The message positioned at a position written by WritePosition.
 * @return The position.
 */
svanes::Vector2D ReadPosition(svanes::MessageReader &reader) {
    const float x = reader.ReadFloat32();
    const float y = reader.ReadFloat32();
    return {x, y};
}

/**
 * @param writer The message to append to.
 * @param clock The clock setting to write.
 */
void WriteClock(svanes::MessageWriter &writer, ClockSetting clock) {
    writer.WriteUint8(static_cast<std::uint8_t>(clock.speed));
    writer.WriteBool(clock.paused);
}

/**
 * @param reader The message positioned at a setting written by WriteClock.
 * @return The clock setting.
 */
ClockSetting ReadClock(svanes::MessageReader &reader) {
    const auto speed = static_cast<GooseSpeed>(reader.ReadUint8());
    const bool paused = reader.ReadBool();
    return {speed, paused};
}

/**
 * @param reader A reader that should have consumed its whole message.
 * @throws std::invalid_argument if any bytes are left unread.
 */
void RequireFullyRead(const svanes::MessageReader &reader) {
    if (reader.Remaining() != 0) {
        throw std::invalid_argument("A goose message had unread bytes left.");
    }
}

} // namespace

svanes::RationalNumber TicSize(GooseSpeed speed) {
    switch (speed) {
    case GooseSpeed::Half:
        return svanes::RationalNumber{2};
    case GooseSpeed::Normal:
        return svanes::RationalNumber{1};
    case GooseSpeed::Double:
        return svanes::RationalNumber{1, 2};
    }
    throw std::invalid_argument("TicSize received an unknown GooseSpeed.");
}

svanes::NetworkMessage EncodePlayerInput(const PlayerInput &input) {
    svanes::MessageWriter writer;
    writer.WriteFloat32(input.intent.move);
    writer.WriteFloat32(input.intent.dash);
    writer.WriteBool(input.intent.jump);
    writer.WriteBool(input.intent.fire);
    WritePosition(writer, input.intent.aim);
    WriteClock(writer, input.clock);
    return writer.Finish();
}

PlayerInput DecodePlayerInput(const svanes::NetworkMessage &message) {
    svanes::MessageReader reader(message);
    PlayerInput input{};
    input.intent.move = reader.ReadFloat32();
    input.intent.dash = reader.ReadFloat32();
    input.intent.jump = reader.ReadBool();
    input.intent.fire = reader.ReadBool();
    input.intent.aim = ReadPosition(reader);
    input.clock = ReadClock(reader);
    RequireFullyRead(reader);
    return input;
}

svanes::NetworkMessage EncodeSnapshot(const Snapshot &snapshot) {
    svanes::MessageWriter writer;
    WriteClock(writer, snapshot.clock);

    writer.WriteUint32(static_cast<std::uint32_t>(snapshot.geese.size()));
    for (const SnapshotGoose &goose : snapshot.geese) {
        writer.WriteUint32(goose.client);
        WritePosition(writer, goose.position);
        writer.WriteUint8(static_cast<std::uint8_t>(goose.state));
    }

    writer.WriteBool(snapshot.enemy.has_value());
    if (snapshot.enemy) {
        WritePosition(writer, *snapshot.enemy);
    }

    writer.WriteUint32(static_cast<std::uint32_t>(snapshot.bullets.size()));
    for (const SnapshotBullet &bullet : snapshot.bullets) {
        WritePosition(writer, bullet.position);
        writer.WriteBool(bullet.from_enemy);
    }
    return writer.Finish();
}

Snapshot DecodeSnapshot(const svanes::NetworkMessage &message) {
    svanes::MessageReader reader(message);
    Snapshot snapshot{};
    snapshot.clock = ReadClock(reader);

    const std::uint32_t goose_count = reader.ReadUint32();
    for (std::uint32_t index = 0; index < goose_count; ++index) {
        const svanes::ClientId client = reader.ReadUint32();
        const svanes::Vector2D position = ReadPosition(reader);
        const auto state = static_cast<GooseState>(reader.ReadUint8());
        snapshot.geese.push_back({client, position, state});
    }

    if (reader.ReadBool()) {
        snapshot.enemy = ReadPosition(reader);
    }

    const std::uint32_t bullet_count = reader.ReadUint32();
    for (std::uint32_t index = 0; index < bullet_count; ++index) {
        const svanes::Vector2D position = ReadPosition(reader);
        const bool from_enemy = reader.ReadBool();
        snapshot.bullets.push_back({position, from_enemy});
    }

    RequireFullyRead(reader);
    return snapshot;
}
