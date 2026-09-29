#pragma once

#include <svanes/network/network_session.hpp>
#include <svanes/network/udp_msg_pipe.hpp>

#include <chrono>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace svanes {

class PeerJoin;

/**
 * Message types PeerGroup sends for itself, in the range reserved for the engine.
 * - SnapshotChunk: One piece of a world snapshot sent to an admitted joiner.
 * - JoinRequest: A contact from a process asking to join, with its rules hash.
 * - JoinPending: A contact reply saying the request is queued for the next
 *   roster change.
 * - JoinRejected: A contact reply giving the reason a request was refused.
 * - JoinAssigned: A contact reply with the joiner's id and the other members'
 *   addresses.
 * - RosterStop: Announces where the sender froze, whether it is leaving, and
 *   whom it admits.
 * - RosterPrepared: Confirms the agreed boundary, world hash, and roster
 *   change.
 */
enum class PeerMessageType : MessageType {
    SnapshotChunk = FirstEngineMessageType,
    JoinRequest,
    JoinPending,
    JoinRejected,
    JoinAssigned,
    RosterStop,
    RosterPrepared
};

/**
 * What every member of a world must agree on.
 * FIELDS:
 * - session: The session id every member uses.
 * - max_players: The most members the world admits.
 * - rules_hash: A hash of the game's fixed rules, which a joiner must match.
 */
struct PeerSettings {
    SessionId session;
    std::uint32_t max_players;
    std::uint64_t rules_hash;
};

/**
 * The identity and address of one member of a peer group.
 * FIELDS:
 * - peer: The member's shared id.
 * - address: The numeric IPv4 address and UDP port on which that member listens.
 */
struct PeerEndpoint {
    PeerId peer;
    UdpAddress address;
};

/**
 * What a member tells a joiner it admitted.
 * FIELDS:
 * - local_peer: The id assigned to the joiner.
 * - sponsor: The member that admitted it, reached through the address the
 *   joiner contacted.
 * - revision: The roster revision that includes the joiner.
 * - peers: Every other member except the joiner and the sponsor.
 */
struct PeerAssignment {
    PeerId local_peer;
    PeerId sponsor;
    std::uint64_t revision = 0;
    std::vector<PeerEndpoint> peers;
};

/**
 * @param assignment The assignment to send.
 * @return The encoded assignment.
 */
NetworkMessage EncodeAssignment(const PeerAssignment &assignment);

/**
 * @param payload An assignment written by EncodeAssignment.
 * @return The assignment it carries.
 * @throws std::invalid_argument for malformed data.
 */
PeerAssignment DecodeAssignment(const NetworkMessage &payload);

/**
 * Interprets an address passed along by another member. A loopback address
 * only means "on the relaying member's computer", so it is replaced by that
 * member's host as this process reaches it, keeping the port.
 * @param relayed The address as the relaying member announced it.
 * @param relay_host The host this process uses to reach the relaying member.
 * @return An address this process can reach.
 */
UdpAddress ResolveRelayedAddress(const UdpAddress &relayed,
                                 const std::string &relay_host);

/**
 * What PeerGroup asks the game's synchronization while it changes the roster.
 * PeerGroup runs the agreement; the participant only answers. Every call
 * happens inside PeerGroup::Update().
 *
 * A roster change freezes every member, agrees on a boundary position (the
 * highest position any member froze at), waits until every member has
 * settled exactly at it, compares hashes of SaveWorld(), and then applies the
 * change on every member together.
 */
class RosterParticipant {
public:
    virtual ~RosterParticipant() = default;

    /**
     * Stops local progress, such as generating new inputs, for a roster
     * change.
     * @return The position local progress stopped at, such as a tick.
     */
    virtual std::uint64_t Freeze() = 0;

    /**
     * Advances toward the agreed boundary without new local progress. Called
     * on every Update() until it returns true, and again after that until
     * every member agrees.
     * @param boundary The position every member stops at.
     * @return Whether this process is exactly at the boundary with every
     * member's progress through it confirmed.
     */
    virtual bool SettleAt(std::uint64_t boundary) = 0;

    /**
     * @return The world as bytes. Members compare hashes of these bytes, and
     * joiners receive them.
     */
    virtual NetworkMessage SaveWorld() = 0;

    /**
     * Applies an agreed change on a member that stays. Peers() already holds
     * the new roster.
     * @param departing The members that left, to remove from the world.
     * @param joining The members that joined, in ascending id order, to add.
     */
    virtual void ChangeRoster(std::span<const PeerId> departing,
                              std::span<const PeerId> joining) = 0;

    /**
     * Rebuilds the world a joiner received. PeerGroup then checks that
     * SaveWorld() hashes to what the sponsor sent.
     * @param roster Every member, in ascending id order.
     * @param world Bytes the sponsor's SaveWorld() produced.
     */
    virtual void LoadWorld(std::span<const PeerId> roster,
                           std::span<const std::byte> world) = 0;
};

/**
 * Connects this process to the other members of a peer-to-peer world and
 * manages who is in it. A world starts with one member, peer 1, and grows as
 * others join through any member.
 *
 * NetworkSession handles delivery. PeerGroup tags broadcast messages with the
 * roster revision and holds back messages from the next revision until the
 * roster changes. It answers join requests, admitting a process only if its
 * rules hash matches, the world has room, and this member is not leaving.
 * Queued joins and departures are applied through one roster change agreed
 * by every member, with the attached RosterParticipant settling the world at
 * a common boundary. Ids come from a counter every member advances the same
 * way, so no id is reused. The member that admitted a joiner sends it an
 * assignment and the world. Failed delivery leaves a status message.
 */
class PeerGroup {
public:
    /**
     * Starts a new world with this process as its only member, peer 1.
     * @param bound_pipe The pipe this process listens on.
     * @param settings What every member must agree on.
     */
    PeerGroup(std::unique_ptr<UdpMsgPipe> bound_pipe, PeerSettings settings);

    /**
     * Joins a world through any of its members. Update() asks that member to
     * admit this process. Once admitted, this process connects to every
     * member it was told about, resolving loopback addresses against the
     * contacted member's host, and the participant loads the world when it
     * arrives.
     * @param bound_pipe The pipe this process listens on.
     * @param entry The host and port of a member.
     * @param settings What every member must agree on.
     */
    PeerGroup(std::unique_ptr<UdpMsgPipe> bound_pipe, UdpAddress entry,
              PeerSettings settings);

    ~PeerGroup();

    /**
     * Sets the synchronization that answers during roster changes.
     * @param roster_participant The participant, which must outlive every
     * later Update().
     */
    void Attach(RosterParticipant &roster_participant);

    /**
     * @return The hash a joiner must match: the game's rules hash combined
     * with the player limit.
     */
    std::uint64_t RulesHash() const;

    /**
     * Pumps delivery, advances joining, loads a received world, answers join
     * requests, and advances roster changes.
     * @throws std::logic_error if no participant is attached.
     * @throws std::invalid_argument for malformed messages or messages beyond
     * the next revision.
     * @throws std::runtime_error if a received world does not match its hash.
     */
    void Update();

    /**
     * Queues a message for every member, tagged with the current revision.
     * @param type The message type, outside PeerGroup's own types.
     * @param payload The serialized data.
     * @return Whether the session accepted the message.
     * @throws std::invalid_argument for one of PeerGroup's own types.
     */
    bool Broadcast(MessageType type, const NetworkMessage &payload);

    /**
     * Takes the next broadcast message for the current roster revision.
     * @param message Receives the sender, type, and payload without the
     * revision tag.
     * @return Whether a message was available and the group has not failed.
     */
    bool Receive(SessionMessage &message);

    /**
     * Leaves at the next roster change, or at once if this process never
     * joined or the group failed.
     */
    void RequestLeave();

    /** @return Whether the other members agreed to this process leaving. */
    bool HasDeparted() const;

    /**
     * @return Whether the process can close: after departing and delivering
     * everything, or at once after a failure or an abandoned join.
     */
    bool CanClose() const;

    /**
     * @return Whether this process is a member with a loaded world and has
     * not departed.
     */
    bool IsRunning() const;

    /** @return Whether a roster change is in progress. */
    bool IsChangingRoster() const;

    /** @return The sorted roster including the local member, empty before admission. */
    std::span<const PeerId> Peers() const;

    /** @return The member this process controls, zero before admission. */
    PeerId LocalPeer() const;

    /** @return The local UDP port others can join through. */
    std::uint16_t Port() const;

    /**
     * @return Whether joining failed, a peer failed, a roster change stalled
     * or disagreed, or a message queue overflowed.
     */
    bool HasFailed() const;

    /** @return A short status for the game's display or console. */
    std::string Status() const;

    /** @return The revision attached to broadcast messages. */
    std::uint64_t Revision() const;

private:
    /**
     * Snapshot chunks waiting for send capacity.
     * FIELDS:
     * - peer: The joiner receiving them.
     * - chunks: Every chunk payload in order.
     * - next: The index of the first chunk not yet accepted by the session.
     */
    struct OutgoingSnapshot {
        PeerId peer;
        std::vector<NetworkMessage> chunks;
        std::size_t next = 0;
    };

    /**
     * A process asking this member to admit it.
     * FIELDS:
     * - connection: The pipe connection its requests arrive on.
     * - address: The host and port it listens on.
     */
    struct JoinCandidate {
        ConnectionId connection;
        UdpAddress address;
    };

    /**
     * An assignment kept so a lost reply can be repeated.
     * FIELDS:
     * - payload: The JoinAssigned message.
     * - sent_at: When the joiner was admitted.
     */
    struct SentAssignment {
        NetworkMessage payload;
        std::chrono::steady_clock::time_point sent_at;
    };

    /**
     * A member's frozen position and the roster changes it brings.
     * FIELDS:
     * - position: Where that member froze.
     * - leaving: Whether that member is departing.
     * - joining: Addresses of the processes that member admits, in request
     *   order.
     */
    struct StopRecord {
        std::uint64_t position;
        bool leaving;
        std::vector<UdpAddress> joining;

        /** @return Whether both records are identical. */
        bool operator==(const StopRecord &) const = default;
    };

    /**
     * A member's confirmation of the world and roster change at the boundary.
     * FIELDS:
     * - boundary: The agreed position.
     * - world_hash: A hash of the participant's world at the boundary.
     * - roster_hash: A hash of the revision, next id, and every stop record.
     */
    struct PreparedRecord {
        std::uint64_t boundary;
        std::uint64_t world_hash;
        std::uint64_t roster_hash;

        /** @return Whether both records are identical. */
        bool operator==(const PreparedRecord &) const = default;
    };

    /**
     * A roster change in progress, with no member in charge.
     * FIELDS:
     * - local_stop: This member's frozen position, departure, and admissions.
     * - sponsored: The processes this member admits in this change.
     * - stops: Each member's stop record, keyed by peer id.
     * - prepared: Each member's confirmation, keyed by peer id.
     * - stop_sent: Whether the session accepted the local stop record.
     * - started_at: When the change began, used to report a stall.
     */
    struct RosterChange {
        StopRecord local_stop;
        std::vector<JoinCandidate> sponsored;
        std::map<std::uint32_t, StopRecord> stops;
        std::map<std::uint32_t, PreparedRecord> prepared;
        bool stop_sent = false;
        std::chrono::steady_clock::time_point started_at;
    };

    /**
     * Records one received snapshot chunk.
     * @param message The SnapshotChunk message.
     * @throws std::invalid_argument for an unexpected or inconsistent chunk.
     */
    void ReadSnapshotChunk(const SessionMessage &message);

    /**
     * Queues a broadcast message by its revision tag.
     * @param message The message with its revision tag still attached.
     * @throws std::invalid_argument for a message beyond the next revision.
     */
    void QueueTaggedMessage(SessionMessage message);

    /**
     * Tags and broadcasts a message of any type, including PeerGroup's own.
     * @param type The message type.
     * @param payload The serialized data.
     * @return Whether the session accepted the message.
     */
    bool BroadcastTagged(MessageType type, const NetworkMessage &payload);

    /**
     * Admits this process under an assignment and connects to its members.
     * @param sponsor_connection The connection to the member that admitted it.
     * @param assignment The id, revision, and members that member sent.
     */
    void Adopt(ConnectionId sponsor_connection,
               const PeerAssignment &assignment);

    /**
     * Loads the world from a complete joiner snapshot and checks its hash.
     * @param snapshot The roster, next id, hash, and world bytes.
     * @throws std::invalid_argument for malformed data.
     * @throws std::runtime_error if the roster or world differs from the
     * sponsor's.
     */
    void LoadJoinSnapshot(const NetworkMessage &snapshot);

    /**
     * Splits a joiner snapshot into chunks for one member.
     * @param peer The admitted joiner.
     * @param snapshot The complete encoded snapshot.
     * @throws std::length_error if the snapshot is empty or too large.
     */
    void SendSnapshot(PeerId peer, const NetworkMessage &snapshot);

    /** Answers join requests and queues acceptable ones for the next change. */
    void HandleJoinRequests();

    /**
     * Refuses a join request.
     * @param connection The requesting process.
     * @param reason The explanation shown to that player.
     */
    void RejectJoin(ConnectionId connection, std::string_view reason);

    /** Takes roster messages out of the current revision's queue. */
    void ProcessRosterMessages();

    /**
     * Records a stop or prepared message, allowing either to arrive first.
     * @param message A current revision roster message.
     * @throws std::invalid_argument for a malformed message or sender.
     */
    void ReceiveRosterMessage(const SessionMessage &message);

    /** Freezes the participant and records the departure and queued joins. */
    void BeginRosterChange();

    /** Advances the agreement and applies the change once every member agrees. */
    void UpdateRosterChange();

    /**
     * Adopts an agreed roster change and advances the roster revision.
     * Retired routes keep acknowledging retries while new traffic uses the
     * new roster.
     * @param departing The members removed at the agreed boundary.
     * @param joining The members added at that boundary, with their addresses.
     * @throws std::overflow_error if the revision is exhausted.
     */
    void ApplyRosterChange(std::span<const PeerId> departing,
                           std::span<const PeerEndpoint> joining);

    /**
     * @param peer A current remote member.
     * @return The address this process uses to reach it.
     * @throws std::invalid_argument if the peer is not a remote member.
     */
    UdpAddress PeerAddress(PeerId peer) const;

    /**
     * @return Every current member except this process, with the address
     * this process uses for it.
     */
    std::vector<PeerEndpoint> RemoteEndpoints() const;

    PeerSettings settings;
    std::unique_ptr<NetworkSession> session;
    UdpMsgPipe *pipe = nullptr;
    RosterParticipant *participant = nullptr;
    std::vector<PeerId> peers;
    std::string failure;
    std::string status;
    std::deque<SessionMessage> messages;
    std::deque<SessionMessage> future_messages;
    std::uint64_t revision = 1;
    std::uint32_t next_peer_id = 2;
    bool world_loaded = false;
    bool leave_requested = false;
    bool departed = false;
    bool force_close = false;
    std::vector<OutgoingSnapshot> outgoing_snapshots;
    std::optional<std::uint32_t> snapshot_chunk_count;
    std::map<std::uint32_t, std::vector<std::byte>> snapshot_chunks;
    std::unique_ptr<PeerJoin> join;
    std::vector<JoinCandidate> join_requests;
    std::map<std::uint32_t, SentAssignment> assignments;
    std::optional<RosterChange> roster_change;
};

} // namespace svanes
