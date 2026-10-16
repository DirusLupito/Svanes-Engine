#include "protocol.hpp"

namespace svanes::internal {

NetworkMessage EncodeJoinRequest(const std::string &reply_address,
                                 const NetworkMessage &request) {
    MessageWriter writer;
    writer.WriteUint8(static_cast<std::uint8_t>(ClientServerMessageKind::Join));
    writer.WriteText(reply_address);
    writer.WriteBytes(request.bytes);
    return writer.Finish();
}

NetworkMessage EncodeJoinResult(bool accepted, std::uint32_t client_id,
                                const NetworkMessage &response) {
    MessageWriter writer;
    writer.WriteUint8(static_cast<std::uint8_t>(
        accepted ? ClientServerMessageKind::Accepted
                 : ClientServerMessageKind::Rejected));
    if (accepted) {
        writer.WriteUint32(client_id);
    }
    writer.WriteBytes(response.bytes);
    return writer.Finish();
}

NetworkMessage EncodeGameMessage(std::uint32_t client_id,
                                 const NetworkMessage &message) {
    MessageWriter writer;
    writer.WriteUint8(static_cast<std::uint8_t>(ClientServerMessageKind::Game));
    writer.WriteUint32(client_id);
    writer.WriteBytes(message.bytes);
    return writer.Finish();
}

NetworkMessage ReadPayload(MessageReader &reader) {
    const auto bytes = reader.ReadBytes(reader.Remaining());
    return NetworkMessage{std::vector<std::byte>(bytes.begin(), bytes.end())};
}

} // namespace svanes::internal
