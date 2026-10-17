#pragma once

#include <svanes/network/message_serialization.hpp>

#include <cstdint>
#include <string>

namespace svanes::internal {

/**
 * All client-server messages types recognized by the engine. Used to identify
 * the type of message being sent or received in order to properly decode the
 * message contents. MUST be used as the first byte of every message sent
 * between a client and server.
 *
 * MEMBERS:
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

/**
 * Encodes a join request message to be sent from a client to the server.
 *
 * @param reply_address The address on which the client is listening for the
 * server's response.
 * @param request The game's join request payload.
 *
 * @return A message containing the encoded join request.
 */
NetworkMessage EncodeJoinRequest(const std::string &reply_address,
                                 const NetworkMessage &request);

/**
 * Encodes a join result message to be sent from the server to a client.
 *
 * @param accepted Whether the client's join request was accepted.
 * @param client_id The nonzero ID assigned to the client if accepted. Ignored
 * if rejected.
 * @param response The server's response payload to the client's join request.
 *
 * @return A message containing the encoded join result.
 */
NetworkMessage EncodeJoinResult(bool accepted, std::uint32_t client_id,
                                const NetworkMessage &response);

/**
 * Encodes a game message to be sent between a client and the server.
 *
 * @param client_id The sender client ID for client-to-server messages,
 * or the recipient client ID for server-to-client messages.
 * @param message The game's message payload.
 *
 * @return A message containing the encoded game message.
 */
NetworkMessage EncodeGameMessage(std::uint32_t client_id,
                                 const NetworkMessage &message);

/**
 * Reads the remaining bytes from a message reader and returns them as a
 * NetworkMessage. Should only be called after the given reader has read
 * the engine header fields of the message, leaving only the game's payload
 * bytes remaining.
 *
 * @param reader The message reader from which to read the payload.
 *
 * @return A NetworkMessage containing the remaining bytes.
 */
NetworkMessage ReadPayload(MessageReader &reader);

} // namespace svanes::internal
