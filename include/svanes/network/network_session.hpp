#pragma once

#include <svanes/network/msg_pipe.hpp>

#include <memory>
#include <span>
#include <vector>

namespace svanes {

/**
 * Identifies a participant consistently across the configured session.
 *
 * FIELDS:
 * - value: The shared peer id. Zero is invalid.
 */
struct PeerId {
    std::uint32_t value = 0;

    /**
     * @return Whether the two peer ids have the same value.
     */
    bool operator==(const PeerId &) const = default;
};

/**
 * A nonzero identifier shared by every participant in the session.
 */
using SessionId = std::uint64_t;

/**
 * Identifies a payload format defined by the game.
 */
using MessageType = std::uint16_t;

/**
 * Leaves 24 bytes for the header within a 1200-byte session message.
 */
inline constexpr std::size_t MaxSessionPayloadBytes = 1176;

/**
 * Associates a remote peer with a connection in this process.
 *
 * FIELDS:
 * - peer: The remote participant's shared id.
 * - connection: The local transport handle used to reach that peer.
 */
struct PeerConnection {
    PeerId peer;
    ConnectionId connection;
};

/**
 * Supplies the local identity and the fixed set of remote participants.
 *
 * FIELDS:
 * - session: The nonzero session id shared by all participants.
 * - local_peer: This process's peer id.
 * - remote_peers: Remote peer-to-connection mappings, excluding the local peer.
 */
struct SessionConfiguration {
    SessionId session;
    PeerId local_peer;
    std::vector<PeerConnection> remote_peers;
};

/**
 * A game message together with its sender and payload type.
 *
 * FIELDS:
 * - sender: The peer that sent the message.
 * - type: The game-defined message type used to interpret the payload.
 * - payload: The message bytes, without the session header.
 */
struct SessionMessage {
    PeerId sender;
    MessageType type;
    NetworkMessage payload;
};

/**
 * Encodes the session header and payload using the portable wire format.
 *
 * @param session The nonzero session id to include.
 * @param message The sender, type, and payload to encode.
 * @return The complete message ready for the transport.
 * @throws std::invalid_argument if the session or sender id is zero.
 * @throws std::length_error if the payload exceeds MaxSessionPayloadBytes.
 */
NetworkMessage EncodeSessionMessage(SessionId session,
                                    const SessionMessage &message);

/**
 * Checks the session header and extracts the sender, type, and payload.
 *
 * @param session The session id the message must belong to.
 * @param message The complete received bytes to decode.
 * @return The decoded message. The game still needs to validate its payload.
 * @throws std::invalid_argument if the header, size, or session id is invalid.
 */
SessionMessage DecodeSessionMessage(SessionId session,
                                    const NetworkMessage &message);

/**
 * Sends and receives game messages through an owned MsgPipe using a fixed
 * peer roster. Each participant has a PeerId shared across the session, and 
 * the roster maps remote peers to the local ConnectionIds used to reach them.
 * 
 * Sending wraps the game's payload with a header identifying the session,
 * sender, and message type, then passes it to the appropriate connection.
 * Receiving checks that header and matches the claimed sender to the roster's
 * connection mapping before returning the payload to the game.
 * 
 * The game defines the payload contents and decides how they affect its world.
 * The caller supplies the roster when constructing the session.
 */
class NetworkSession {
public:
    /**
     * Takes ownership of a pipe and validates its configured peer mappings.
     *
     * @param pipe The transport whose connection handles appear in the roster.
     * @param configuration The session id, local peer, and remote mappings.
     * @throws std::invalid_argument for a null pipe or invalid/duplicate mappings.
     */
    NetworkSession(std::unique_ptr<MsgPipe> pipe,
                   SessionConfiguration configuration);

    /**
     * @return The peer id representing this process.
     */
    PeerId LocalPeer() const;

    /**
     * @return A read-only view of the remote mappings, borrowed from the session.
     */
    std::span<const PeerConnection> RemotePeers() const;

    /**
     * Wraps a payload with the local identity and sends it to one peer.
     *
     * @param destination The configured peer to send to.
     * @param type The game-defined payload type.
     * @param payload The bytes to send, up to MaxSessionPayloadBytes.
     * @return Whether the transport accepted the send locally.
     * @throws std::invalid_argument if the destination is outside the roster.
     * @throws std::length_error if the payload is too large.
     */
    bool Send(PeerId destination, MessageType type,
              const NetworkMessage &payload);

    /**
     * Sends a payload to every configured remote peer.
     *
     * @param type The game-defined payload type.
     * @param payload The bytes to send, up to MaxSessionPayloadBytes.
     * @return Whether every send was accepted locally; true for an empty roster.
     * @throws std::length_error if the payload is too large.
     */
    bool Broadcast(MessageType type, const NetworkMessage &payload);

    /**
     * Receives a message and checks its sender against the connection mapping.
     *
     * @param received Receives the validated envelope and its payload.
     * @return Whether a message was available.
     * @throws std::invalid_argument for invalid envelopes or unrecognized senders.
     */
    bool Receive(SessionMessage &received);

private:
    std::unique_ptr<MsgPipe> pipe;
    SessionConfiguration configuration;
};

}
