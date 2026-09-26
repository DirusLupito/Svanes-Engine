#pragma once

#include <svanes/network/msg_pipe.hpp>

#include <chrono>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <set>
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
 * Leaves 31 bytes for the header within a 1200-byte session packet.
 * The 1200-byte max is taken from Gaffer to avoid IP fragmentation.
 * More information can be found below:
 * https://gafferongames.com/post/packet_fragmentation_and_reassembly/
 */
inline constexpr std::size_t MaxSessionPayloadBytes = 1169;

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
 * Controls retry timing and the amount of data retained by a session.
 *
 * FIELDS:
 * - retry_interval: Real time between send attempts for an unacknowledged message.
 * - delivery_timeout: Real time from the first send attempt before reporting failure.
 * - max_pending_per_peer: Maximum pending count and outgoing message id span per peer.
 *   Also bounds how far ahead of a missing message incoming ids are accepted.
 * - max_incoming_messages: Maximum game messages waiting for Receive(), across all peers.
 * - max_packets_per_update: Maximum incoming packets processed per Update(), including
 *   acknowledgments and duplicates, before handling outgoing traffic and returning.
 */
struct ReliabilitySettings {
    std::chrono::milliseconds retry_interval{100};
    std::chrono::milliseconds delivery_timeout{5000};
    std::uint32_t max_pending_per_peer = 256;
    std::uint32_t max_incoming_messages = 1024;
    std::uint32_t max_packets_per_update = 1024;
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
 * Identifies a data message within one sender-to-recipient stream. Zero is invalid.
 */
using MessageId = std::uint64_t;

/**
 * Identifies what a session packet carries.
 * - Data: A game message with an id assigned by its sender.
 * - Acknowledgment: Confirmation that the recipient accepted the given message id.
 */
enum class SessionPacketKind : std::uint8_t {
    Data = 1,
    Acknowledgment = 2
};

/**
 * The session's wire representation of a game message or acknowledgment.
 *
 * FIELDS:
 * - kind: Whether this packet carries data or acknowledges a data message.
 * - sender: The peer sending this packet, including for acknowledgments.
 * - message_id: The data message's id, echoed back by its acknowledgment.
 * - type: The game-defined payload type for data, or zero for acknowledgments.
 * - payload: The game bytes for data, or empty for acknowledgments.
 */
struct SessionPacket {
    SessionPacketKind kind;
    PeerId sender;
    MessageId message_id;
    MessageType type;
    NetworkMessage payload;
};

/**
 * Encodes a data packet or acknowledgment using the portable wire format.
 *
 * @param session The nonzero session id to include.
 * @param packet The packet kind, identities, and optional game payload.
 * @return The complete packet ready for the transport.
 * @throws std::invalid_argument for zero ids, an unknown kind, or an acknowledgment
 * carrying a game type or payload.
 * @throws std::length_error if the payload exceeds MaxSessionPayloadBytes.
 */
NetworkMessage EncodeSessionPacket(SessionId session,
                                   const SessionPacket &packet);

/**
 * Checks the session header and extracts a data packet or acknowledgment.
 *
 * @param session The session id the message must belong to.
 * @param message The complete received bytes to decode.
 * @return The decoded packet. The game still needs to validate data payloads.
 * @throws std::invalid_argument if the header, size, ids, kind, or acknowledgment
 * contents are invalid.
 */
SessionPacket DecodeSessionPacket(SessionId session,
                                  const NetworkMessage &message);

/**
 * Sends and receives game messages through an owned MsgPipe using a configured
 * peer roster. Each participant has a PeerId shared across the session, and 
 * the roster maps remote peers to the local ConnectionIds used to reach them.
 * 
 * Send() retains a packet with its own per-peer message id. Update() transmits
 * it and retries until the destination acknowledges it or delivery times out.
 * Incoming data is checked against the roster, acknowledged, and queued for
 * Receive(). Duplicate packets are acknowledged again but queued only once.
 * Messages are available in arrival order without waiting for missing ids.
 *
 * Retry timing uses a monotonic real-time clock. Call Update() each frame,
 * including while gameplay is paused, on the thread that created the pipe.
 * A delivery timeout stops traffic to that peer and queues one failure report
 * for the game. The roster and already-queued incoming messages are preserved.
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
     * @param settings The retry intervals and queue limits.
     * @throws std::invalid_argument for a null pipe, invalid/duplicate mappings,
     * nonpositive limits or intervals, or a timeout shorter than the retry interval.
     */
    NetworkSession(std::unique_ptr<MsgPipe> pipe,
                   SessionConfiguration configuration,
                   ReliabilitySettings settings = {});

    /**
     * @return The peer id representing this process.
     */
    PeerId LocalPeer() const;

    /**
     * @return Known remote mappings, including retired routes, borrowed from the session.
     */
    std::span<const PeerConnection> RemotePeers() const;

    /**
     * Queues a payload for delivery to one peer. Update() handles transmission.
     *
     * @param destination The configured peer to send to.
     * @param type The game-defined payload type.
     * @param payload The bytes to send, up to MaxSessionPayloadBytes.
     * @return Whether the session accepted the message. Returns false for a failed peer
     * or a full send window. Acceptance does not confirm delivery.
     * @throws std::invalid_argument if the destination is outside the roster.
     * @throws std::length_error if the payload is too large.
     * @throws std::overflow_error if this peer's message ids are exhausted.
     */
    bool Send(PeerId destination, MessageType type,
              const NetworkMessage &payload);

    /**
     * Queues a payload for every configured remote peer after checking that
     * all peers can accept it. A false return queues nothing.
     *
     * @param type The game-defined payload type.
     * @param payload The bytes to send, up to MaxSessionPayloadBytes.
     * @return Whether every peer's queue accepted the message. Returns true for an empty roster.
     * @throws std::length_error if the payload is too large.
     * @throws std::overflow_error if a peer's message ids are exhausted.
     */
    bool Broadcast(MessageType type, const NetworkMessage &payload);

    /**
     * Processes incoming packets, acknowledgments, outgoing sends, and retries.
     * Reads at most max_packets_per_update packets so incoming traffic cannot
     * indefinitely delay outgoing work or the next game frame.
     * New data is acknowledged only after entering the game-message queue.
     * Expired deliveries mark their peer as failed and queue one failure report.
     * Call each frame, independently of simulation time or pause state.
     * @throws std::invalid_argument for invalid packets or unrecognized senders.
     */
    void Update();

    /**
     * Takes the next game message accepted by Update().
     *
     * @param received Receives the sender, game type, and payload when available.
     * @return Whether a queued game message was available.
     */
    bool Receive(SessionMessage &received);

    /**
     * Takes the next peer failure reported by Update(). Each peer is reported once.
     * @param peer Receives the failed peer's id when a report is available.
     * @return Whether a failure report was available.
     */
    bool ReceivePeerFailure(PeerId &peer);

    /**
     * Stops new traffic to a departed peer while draining accepted sends.
     * Late data is acknowledged and discarded so departure retries can finish.
     * @param peer The configured remote identity to retire.
     * @throws std::invalid_argument if the peer is not configured.
     */
    void RetirePeer(PeerId peer);

    /** @return Whether every accepted outgoing message has left the retry queues. */
    bool OutgoingDrained() const;


private:
    /**
     * Retains one outgoing packet until its acknowledgment arrives.
     * FIELDS:
     * - packet: Encoded bytes reused for each send attempt.
     * - first_attempt: Time of the first attempt, or empty before transmission.
     * - last_attempt: Time of the most recent attempt, used after first_attempt is set.
     */
    struct PendingMessage {
        NetworkMessage packet;
        std::optional<std::chrono::steady_clock::time_point> first_attempt;
        std::chrono::steady_clock::time_point last_attempt{};
    };

    /**
     * Tracks delivery separately for one entry in the configured remote roster.
     * FIELDS:
     * - next_message_id: Next outgoing id. Zero marks an exhausted sequence.
     * - pending: Outgoing packets indexed by message id.
     * - received_through: Highest incoming id with every preceding id accepted.
     * - received_ahead: Accepted incoming ids above a gap, retained for duplicate checks.
     * - failed: Whether delivery timed out and further traffic is stopped.
     * - retired: Whether new sends and game delivery are disabled after departure.
     */
    struct PeerState {
        MessageId next_message_id = 1;
        std::map<MessageId, PendingMessage> pending;
        MessageId received_through = 0;
        std::set<MessageId> received_ahead;
        bool failed = false;
        bool retired = false;
    };

    /**
     * Checks failure status, queue capacity, and distance from the oldest pending id.
     * @param peer The peer state to inspect.
     * @return Whether another outgoing message can be accepted.
     * @throws std::overflow_error if the message id sequence is exhausted.
     */
    bool CanQueue(const PeerState &peer) const;

    /**
     * Stores an encoded packet and advances the peer's outgoing id sequence.
     * @param peer A peer whose send window has already been checked.
     * @param type The game-defined message type.
     * @param payload The game bytes to encode.
     */
    void QueueMessage(PeerState &peer, MessageType type, const NetworkMessage &payload);

    /**
     * Processes a validated packet from a configured peer.
     * @param index The peer's index in the roster and state array.
     * @param packet The packet whose sender has been checked against its connection.
     */
    void ProcessPacket(std::size_t index, SessionPacket packet);

    std::unique_ptr<MsgPipe> pipe;
    SessionConfiguration configuration;
    ReliabilitySettings settings;
    std::vector<PeerState> peers;
    std::deque<SessionMessage> incoming_messages;
    std::deque<PeerId> failed_peers;
};

}
