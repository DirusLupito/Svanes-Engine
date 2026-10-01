#include "server_protocol.hpp"

#include <svanes/network/message_serialization.hpp>

#include <stdexcept>
#include <string>

namespace {

/**
 * @param type The message type.
 * @return A writer that has already written the type.
 */
svanes::MessageWriter StartMessage(GooseMessageType type)
{
    svanes::MessageWriter writer;
    writer.WriteUint8(static_cast<std::uint8_t>(type));
    return writer;
}

/**
 * @param reader The message positioned at a speed.
 * @return The speed.
 * @throws std::invalid_argument for a value outside GooseSpeed.
 */
GooseSpeed ReadSpeed(svanes::MessageReader& reader)
{
    const std::uint8_t value = reader.ReadUint8();
    if (value > static_cast<std::uint8_t>(GooseSpeed::Double)) {
        throw std::invalid_argument("A goose message carried an unknown speed.");
    }
    return static_cast<GooseSpeed>(value);
}

}

svanes::NetworkMessage EncodeHello(svanes::ClientId client)
{
    svanes::MessageWriter writer = StartMessage(GooseMessageType::Hello);
    writer.WriteUint32(client);
    return writer.Finish();
}

svanes::NetworkMessage EncodeWelcome(svanes::ClientId client, svanes::PeerId peer)
{
    svanes::MessageWriter writer = StartMessage(GooseMessageType::Welcome);
    writer.WriteUint32(client);
    writer.WriteUint32(peer.value);
    return writer.Finish();
}

svanes::NetworkMessage EncodeTaken(svanes::ClientId client)
{
    svanes::MessageWriter writer = StartMessage(GooseMessageType::Taken);
    writer.WriteUint32(client);
    return writer.Finish();
}

svanes::NetworkMessage EncodeInput(const GooseSimulation& simulation, const GooseIntent& intent, ClockRequest clock)
{
    svanes::MessageWriter writer = StartMessage(GooseMessageType::Input);
    simulation.EncodeInput(writer, intent);
    writer.WriteUint8(static_cast<std::uint8_t>(clock.speed));
    writer.WriteBool(clock.paused);
    return writer.Finish();
}

svanes::NetworkMessage EncodeSnapshot(std::span<const svanes::PeerId> roster, const svanes::NetworkMessage& world)
{
    svanes::MessageWriter writer = StartMessage(GooseMessageType::Snapshot);
    writer.WriteUint32(static_cast<std::uint32_t>(roster.size()));
    for (const svanes::PeerId peer : roster) {
        writer.WriteUint32(peer.value);
    }
    writer.WriteBytes(world.bytes);
    return writer.Finish();
}

svanes::NetworkMessage EncodeGoodbye()
{
    return StartMessage(GooseMessageType::Goodbye).Finish();
}

GooseMessage DecodeGooseMessage(const svanes::NetworkMessage& message, const GooseSimulation& simulation)
{
    svanes::MessageReader reader(message);
    const std::uint8_t type = reader.ReadUint8();
    if (type > static_cast<std::uint8_t>(GooseMessageType::Goodbye)) {
        throw std::invalid_argument("Received a goose message of unknown type " + std::to_string(type) + ".");
    }
    GooseMessage decoded{.type = static_cast<GooseMessageType>(type)};
    switch (decoded.type) {
    case GooseMessageType::Hello:
    case GooseMessageType::Taken:
        decoded.client = reader.ReadUint32();
        break;
    case GooseMessageType::Welcome:
        decoded.client = reader.ReadUint32();
        decoded.peer.value = reader.ReadUint32();
        break;
    case GooseMessageType::Input:
        decoded.intent = simulation.DecodeInput(reader);
        decoded.clock.speed = ReadSpeed(reader);
        decoded.clock.paused = reader.ReadBool();
        break;
    case GooseMessageType::Snapshot: {
        const std::uint32_t count = reader.ReadUint32();
        if (count > GooseServerSlots) {
            throw std::invalid_argument("A goose snapshot named more players than a server holds.");
        }
        for (std::uint32_t index = 0; index < count; ++index) {
            decoded.roster.push_back({reader.ReadUint32()});
        }
        const auto world = reader.ReadBytes(reader.Remaining());
        decoded.world.assign(world.begin(), world.end());
        break;
    }
    case GooseMessageType::Goodbye:
        break;
    }
    if (reader.Remaining() != 0) {
        throw std::invalid_argument("A goose message had unread bytes left over.");
    }
    return decoded;
}
