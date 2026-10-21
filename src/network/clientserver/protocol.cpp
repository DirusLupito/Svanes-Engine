#include "protocol.hpp"

namespace svanes::internal {

NetworkMessage EncodeJoinRequest(const NetworkMessage &request) {
    MessageWriter writer;

    // A join request message consists of the following fields, in order:
    // - 1 byte: ClientServerMessageKind::Join
    // - remaining bytes: the join request payload

    writer.WriteUint8(static_cast<std::uint8_t>(ClientServerMessageKind::Join));
    writer.WriteBytes(request.bytes);
    return writer.Finish();
}

NetworkMessage EncodeJoinResult(bool accepted, std::uint32_t client_id,
                                const NetworkMessage &response) {
    MessageWriter writer;

    // A join result message consists of the following fields, in order:
    // - 1 byte: ClientServerMessageKind::Accepted or
    // ClientServerMessageKind::Rejected
    // - 4 bytes: the assigned client ID (only if accepted)
    // - remaining bytes: the server's response payload to the client's join
    // request

    writer.WriteUint8(static_cast<std::uint8_t>(
        accepted ? ClientServerMessageKind::Accepted
                 : ClientServerMessageKind::Rejected));

    if (accepted) {
        writer.WriteUint32(client_id);
    }

    writer.WriteBytes(response.bytes);
    return writer.Finish();
}

NetworkMessage EncodeGameMessage(const NetworkMessage &message,
                                 std::uint32_t client_id) {
    MessageWriter writer;

    // A game message consists of the following fields, in order:
    // - 1 byte: ClientServerMessageKind::Game
    // - 4 bytes: the assigned client ID, or zero if sent by the server
    // - remaining bytes: the game's message payload

    writer.WriteUint8(static_cast<std::uint8_t>(ClientServerMessageKind::Game));

    writer.WriteUint32(client_id);

    writer.WriteBytes(message.bytes);
    return writer.Finish();
}

DecodedMessage DecodeMessage(const NetworkMessage &message) {
    MessageReader reader(message);
    const auto kind = static_cast<ClientServerMessageKind>(reader.ReadUint8());

    // Acceptance and gameplay messages carry a client ID after the message
    // kind. Server gameplay messages use zero for this field.
    std::uint32_t client_id = 0;
    if (kind == ClientServerMessageKind::Accepted ||
        kind == ClientServerMessageKind::Game) {
        client_id = reader.ReadUint32();
    }

    // Everything after the engine fields is the game's payload.
    const auto bytes = reader.ReadBytes(reader.Remaining());
    return {kind, client_id,
            NetworkMessage{std::vector<std::byte>(bytes.begin(), bytes.end())}};
}

} // namespace svanes::internal
