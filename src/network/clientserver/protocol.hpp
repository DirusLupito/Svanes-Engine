#pragma once

#include <svanes/network/message_serialization.hpp>

#include <cstdint>

namespace svanes::internal {

/**
 * All client-server messages types recognized by the engine. Used to identify
 * the type of message being sent or received in order to properly decode the
 * message contents. MUST be used as the first byte of every message sent
 * between a client and server.
 *
 * MEMBERS:
 * - Join: Enum used by a client to request joining a server.
 * - Accepted: Enum used by a server to accept a client's join request.
 * - Rejected: Enum used by a server to reject a client's join request.
 * - Game: Enum used by both a client and server to send gameplay messages.
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
 * @param request The game's join request payload.
 *
 * @return A message containing the encoded join request.
 */
NetworkMessage EncodeJoinRequest(const NetworkMessage &request);

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
 * @param message The game's message payload.
 * @param client_id The assigned client ID for messages from a client to the
 * server. Zero for messages from the server to a client.
 *
 * @return A message containing the encoded game message.
 */
NetworkMessage EncodeGameMessage(const NetworkMessage &message,
                                 std::uint32_t client_id = 0);

/**
 * Represents a decoded client-server message.
 *
 * FIELDS:
 * - kind: The type of engine message received.
 * - client_id: The assigned client ID in an acceptance or gameplay message.
 * Zero for server gameplay messages and messages without a client ID field.
 * - payload: The game's message bytes, without the engine protocol fields.
 */
struct DecodedMessage {
    ClientServerMessageKind kind;
    std::uint32_t client_id;
    NetworkMessage payload;
};

/**
 * Decodes the engine fields and game payload of a client-server message.
 *
 * @param message The complete message received from the pipe.
 *
 * @return The decoded message kind, client ID, and game payload.
 *
 * @throws std::invalid_argument if an engine field is truncated.
 */
DecodedMessage DecodeMessage(const NetworkMessage &message);

} // namespace svanes::internal
