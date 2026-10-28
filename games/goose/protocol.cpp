#include "protocol.hpp"

#include <svanes/network/message_serialization.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

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

/**
 * Appends a number and a separating space to text.
 *
 * @param text The text to append to.
 * @param value The number to write.
 */
template <typename Number> void AppendNumber(std::string &text, Number value) {
    std::array<char, 32> buffer{};
    const auto result =
        std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    text.append(buffer.data(), result.ptr);
    text.push_back(' ');
}

/**
 * Reads space separated numbers from text, in order.
 */
class TextReader {
public:
    /**
     * @param text The text to read.
     */
    explicit TextReader(std::string_view text) : rest(text) {}

    /**
     * @return The next number.
     * @throws std::invalid_argument if the next token is not a number.
     */
    template <typename Number> Number Read() {
        rest.remove_prefix(std::min(rest.find_first_not_of(' '), rest.size()));
        Number value{};
        const auto result =
            std::from_chars(rest.data(), rest.data() + rest.size(), value);
        if (result.ec != std::errc{}) {
            throw std::invalid_argument("A goose text message is malformed.");
        }
        rest.remove_prefix(static_cast<std::size_t>(result.ptr - rest.data()));
        return value;
    }

    /**
     * @throws std::invalid_argument if anything but spaces is left unread.
     */
    void RequireFullyRead() const {
        if (rest.find_first_not_of(' ') != std::string_view::npos) {
            throw std::invalid_argument(
                "A goose text message had unread text left.");
        }
    }

private:
    std::string_view rest;
};

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
    writer.WriteBool(input.minion_spawning.has_value());
    if (input.minion_spawning) {
        writer.WriteBool(*input.minion_spawning);
    }
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
    if (reader.ReadBool()) {
        input.minion_spawning = reader.ReadBool();
    }
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
        writer.WriteUint32(goose.health);
    }

    writer.WriteBool(snapshot.enemy.has_value());
    if (snapshot.enemy) {
        WritePosition(writer, snapshot.enemy->position);
        writer.WriteUint32(snapshot.enemy->health);
    }

    writer.WriteUint32(static_cast<std::uint32_t>(snapshot.bullets.size()));
    for (const SnapshotBullet &bullet : snapshot.bullets) {
        WritePosition(writer, bullet.position);
        writer.WriteBool(bullet.from_enemy);
    }

    writer.WriteUint32(static_cast<std::uint32_t>(snapshot.minions.size()));
    for (const SnapshotMinion &minion : snapshot.minions) {
        WritePosition(writer, minion.position);
        writer.WriteUint8(static_cast<std::uint8_t>(minion.kind));
        writer.WriteUint32(minion.health);
    }
    writer.WriteBool(snapshot.minion_spawning);
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
        const std::uint32_t health = reader.ReadUint32();
        snapshot.geese.push_back({client, position, state, health});
    }

    if (reader.ReadBool()) {
        const svanes::Vector2D position = ReadPosition(reader);
        const std::uint32_t health = reader.ReadUint32();
        snapshot.enemy = SnapshotEnemy{position, health};
    }

    const std::uint32_t bullet_count = reader.ReadUint32();
    for (std::uint32_t index = 0; index < bullet_count; ++index) {
        const svanes::Vector2D position = ReadPosition(reader);
        const bool from_enemy = reader.ReadBool();
        snapshot.bullets.push_back({position, from_enemy});
    }

    const std::uint32_t minion_count = reader.ReadUint32();
    for (std::uint32_t index = 0; index < minion_count; ++index) {
        const svanes::Vector2D position = ReadPosition(reader);
        const auto kind = static_cast<MinionKind>(reader.ReadUint8());
        const std::uint32_t health = reader.ReadUint32();
        snapshot.minions.push_back({position, kind, health});
    }
    snapshot.minion_spawning = reader.ReadBool();

    RequireFullyRead(reader);
    return snapshot;
}

svanes::NetworkMessage EncodeSnapshotText(const Snapshot &snapshot) {
    std::string text;
    AppendNumber(text, static_cast<std::uint32_t>(snapshot.clock.speed));
    AppendNumber(text, static_cast<std::uint32_t>(snapshot.clock.paused));

    AppendNumber(text, static_cast<std::uint32_t>(snapshot.geese.size()));
    for (const SnapshotGoose &goose : snapshot.geese) {
        AppendNumber(text, goose.client);
        AppendNumber(text, goose.position.x);
        AppendNumber(text, goose.position.y);
        AppendNumber(text, static_cast<std::uint32_t>(goose.state));
        AppendNumber(text, goose.health);
    }

    AppendNumber(text, static_cast<std::uint32_t>(snapshot.enemy.has_value()));
    if (snapshot.enemy) {
        AppendNumber(text, snapshot.enemy->position.x);
        AppendNumber(text, snapshot.enemy->position.y);
        AppendNumber(text, snapshot.enemy->health);
    }

    AppendNumber(text, static_cast<std::uint32_t>(snapshot.bullets.size()));
    for (const SnapshotBullet &bullet : snapshot.bullets) {
        AppendNumber(text, bullet.position.x);
        AppendNumber(text, bullet.position.y);
        AppendNumber(text, static_cast<std::uint32_t>(bullet.from_enemy));
    }

    AppendNumber(text, static_cast<std::uint32_t>(snapshot.minions.size()));
    for (const SnapshotMinion &minion : snapshot.minions) {
        AppendNumber(text, minion.position.x);
        AppendNumber(text, minion.position.y);
        AppendNumber(text, static_cast<std::uint32_t>(minion.kind));
        AppendNumber(text, minion.health);
    }
    AppendNumber(text, static_cast<std::uint32_t>(snapshot.minion_spawning));

    svanes::NetworkMessage message;
    message.bytes.resize(text.size());
    std::memcpy(message.bytes.data(), text.data(), text.size());
    return message;
}

Snapshot DecodeSnapshotText(const svanes::NetworkMessage &message) {
    TextReader reader(std::string_view{
        reinterpret_cast<const char *>(message.bytes.data()),
        message.bytes.size()});
    Snapshot snapshot{};
    snapshot.clock.speed =
        static_cast<GooseSpeed>(reader.Read<std::uint32_t>());
    snapshot.clock.paused = reader.Read<std::uint32_t>() != 0;

    const auto goose_count = reader.Read<std::uint32_t>();
    for (std::uint32_t index = 0; index < goose_count; ++index) {
        const auto client = reader.Read<std::uint32_t>();
        const auto x = reader.Read<float>();
        const auto y = reader.Read<float>();
        const auto state = static_cast<GooseState>(reader.Read<std::uint32_t>());
        const auto health = reader.Read<std::uint32_t>();
        snapshot.geese.push_back({client, {x, y}, state, health});
    }

    if (reader.Read<std::uint32_t>() != 0) {
        const auto x = reader.Read<float>();
        const auto y = reader.Read<float>();
        const auto health = reader.Read<std::uint32_t>();
        snapshot.enemy = SnapshotEnemy{{x, y}, health};
    }

    const auto bullet_count = reader.Read<std::uint32_t>();
    for (std::uint32_t index = 0; index < bullet_count; ++index) {
        const auto x = reader.Read<float>();
        const auto y = reader.Read<float>();
        const bool from_enemy = reader.Read<std::uint32_t>() != 0;
        snapshot.bullets.push_back({{x, y}, from_enemy});
    }

    const auto minion_count = reader.Read<std::uint32_t>();
    for (std::uint32_t index = 0; index < minion_count; ++index) {
        const auto x = reader.Read<float>();
        const auto y = reader.Read<float>();
        const auto kind = static_cast<MinionKind>(reader.Read<std::uint32_t>());
        const auto health = reader.Read<std::uint32_t>();
        snapshot.minions.push_back({{x, y}, kind, health});
    }
    snapshot.minion_spawning = reader.Read<std::uint32_t>() != 0;

    reader.RequireFullyRead();
    return snapshot;
}
