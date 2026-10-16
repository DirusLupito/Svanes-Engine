#pragma once

#include <svanes/network/message_serialization.hpp>

#include <cstdint>
#include <string>

namespace svanes::internal {

/**
 * Engine envelopes carried by the shared client-server pipes.
 * - Join: A reply address followed by the game's join request.
 * - Accepted: An assigned client ID followed by the game's response.
 * - Rejected: The game's rejection response.
 * - Game: A client ID followed by opaque game bytes.
 */
enum class ClientServerMessageKind : std::uint8_t {
    Join = 1,
    Accepted,
    Rejected,
    Game
};

NetworkMessage EncodeJoinRequest(const std::string &reply_address,
                                 const NetworkMessage &request);
NetworkMessage EncodeJoinResult(bool accepted, std::uint32_t client_id,
                                const NetworkMessage &response);
NetworkMessage EncodeGameMessage(std::uint32_t client_id,
                                 const NetworkMessage &message);
NetworkMessage ReadPayload(MessageReader &reader);

} // namespace svanes::internal
