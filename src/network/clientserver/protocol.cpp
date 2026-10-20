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

NetworkMessage EncodeGameMessage(const NetworkMessage &message) {
    MessageWriter writer;

    // A game message consists of the following fields, in order:
    // - 1 byte: ClientServerMessageKind::Game
    // - remaining bytes: the game's message payload

    writer.WriteUint8(static_cast<std::uint8_t>(ClientServerMessageKind::Game));
    writer.WriteBytes(message.bytes);
    return writer.Finish();
}

NetworkMessage ReadPayload(MessageReader &reader) {
    const auto bytes = reader.ReadBytes(reader.Remaining());
    return NetworkMessage{std::vector<std::byte>(bytes.begin(), bytes.end())};
}

} // namespace svanes::internal
