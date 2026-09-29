#include "peer_join.hpp"

#include <svanes/network/message_serialization.hpp>

#include <stdexcept>
#include <utility>

namespace svanes {

namespace {

constexpr auto kJoinRequestInterval = std::chrono::milliseconds(250);
constexpr auto kJoinTimeout = std::chrono::seconds(15);

} // namespace

PeerJoin::PeerJoin(NetworkSession &session, UdpMsgPipe &pipe, UdpAddress entry,
                   std::uint64_t rules_hash)
    : session(session), entry(std::move(entry)), rules_hash(rules_hash),
      started_at(std::chrono::steady_clock::now()) {
    entry_connection = pipe.AddRemote(this->entry.host, this->entry.port);
}

void PeerJoin::Update() {
    if (assignment || !failure.empty()) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (now - started_at > kJoinTimeout) {
        failure = answered ? "Not admitted within 15 seconds."
                           : "No reply from " + entry.host + ":" +
                                 std::to_string(entry.port) +
                                 ". Check the address and that the game is "
                                 "running there.";
        return;
    }
    if (!last_request || now - *last_request >= kJoinRequestInterval) {
        MessageWriter writer;
        writer.WriteUint64(rules_hash);
        static_cast<void>(session.SendStranger(
            entry_connection,
            static_cast<MessageType>(PeerMessageType::JoinRequest),
            writer.Finish()));
        last_request = now;
    }
    StrangerMessage stranger;
    while (!assignment && failure.empty() &&
           session.ReceiveStranger(stranger)) {
        if (stranger.connection != entry_connection || !stranger.contact) {
            continue;
        }
        const auto &packet = stranger.message;
        MessageReader reader(packet.payload);
        switch (static_cast<PeerMessageType>(packet.type)) {
        case PeerMessageType::JoinPending:
            answered = true;
            break;
        case PeerMessageType::JoinRejected:
            failure = "Join rejected: " + reader.ReadText();
            break;
        case PeerMessageType::JoinAssigned:
            assignment = DecodeAssignment(packet.payload);
            if (assignment->sponsor != packet.sender) {
                throw std::invalid_argument(
                    "Peer assignment names a different sponsor than its "
                    "sender.");
            }
            break;
        default:
            throw std::invalid_argument(
                "PeerJoin received an unexpected contact message.");
        }
    }
}

const std::optional<PeerAssignment> &PeerJoin::Assignment() const {
    return assignment;
}

ConnectionId PeerJoin::EntryConnection() const { return entry_connection; }

const std::string &PeerJoin::Failure() const { return failure; }

std::string PeerJoin::Status() const {
    if (!failure.empty()) {
        return failure;
    }
    if (assignment) {
        return "Admitted as peer " +
               std::to_string(assignment->local_peer.value) +
               ". Receiving the world.";
    }
    return answered ? "Waiting to be admitted."
                    : "Contacting " + entry.host + ":" +
                          std::to_string(entry.port) + ".";
}

} // namespace svanes
