#include "goose_network.hpp"

#include "goose_simulation.hpp"

#include <svanes/network/message_serialization.hpp>
#include <svanes/network/udp_msg_pipe.hpp>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

GooseNetworkConfiguration MakeLoopbackGooseConfiguration(
    svanes::PeerId local_peer, std::uint32_t player_count,
    std::uint16_t port_base, svanes::SessionId session)
{
    if (session == 0 || local_peer.value == 0 || local_peer.value > player_count ||
        player_count == 0 || player_count > 65535U - port_base) {
        throw std::invalid_argument("Goose roster requires a nonzero session, a local id in the player range, and ports below 65536.");
    }
    GooseNetworkConfiguration configuration{session, local_peer, {}};
    configuration.peers.reserve(player_count);
    for (std::uint32_t id = 1; id <= player_count; ++id) {
        configuration.peers.push_back({{id}, "127.0.0.1", static_cast<std::uint16_t>(port_base + id)});
    }
    return configuration;
}

GooseNetwork::GooseNetwork(GooseNetworkConfiguration configuration)
{
    if (configuration.session == 0 || configuration.local_peer.value == 0 ||
        configuration.peers.empty() || configuration.peers.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument("GooseNetwork requires a session, a local identity, and a nonempty roster.");
    }
    std::sort(configuration.peers.begin(), configuration.peers.end(),
        [](const auto& a, const auto& b) { return a.peer.value < b.peer.value; });
    for (std::size_t index = 0; index < configuration.peers.size(); ++index) {
        const auto& endpoint = configuration.peers[index];
        if (endpoint.peer.value == 0 || endpoint.port == 0 || endpoint.host.empty() ||
            (index > 0 && endpoint.peer == configuration.peers[index - 1].peer)) {
            throw std::invalid_argument("GooseNetwork requires unique nonzero peer ids and valid endpoints.");
        }
        peers.push_back(endpoint.peer);
    }
    const auto local = std::find_if(configuration.peers.begin(), configuration.peers.end(),
        [&](const auto& endpoint) { return endpoint.peer == configuration.local_peer; });
    if (local == configuration.peers.end()) {
        throw std::invalid_argument("GooseNetwork local peer is not in the roster.");
    }
    auto pipe = std::make_unique<svanes::UdpMsgPipe>(local->port);
    const auto local_connection = pipe->AddRemote(local->host, local->port);
    svanes::SessionConfiguration session_configuration{
        configuration.session, configuration.local_peer, {}};
    for (const auto& endpoint : configuration.peers) {
        if (endpoint.peer != configuration.local_peer) {
            const auto connection = pipe->AddRemote(endpoint.host, endpoint.port);
            if (connection == local_connection) {
                throw std::invalid_argument("GooseNetwork remote peer uses the local endpoint.");
            }
            session_configuration.remote_peers.push_back({endpoint.peer, connection});
        }
    }
    session = std::make_unique<svanes::NetworkSession>(std::move(pipe), std::move(session_configuration));
    ready_hashes.resize(peers.size());

    svanes::MessageWriter writer;
    writer.WriteUint32(static_cast<std::uint32_t>(peers.size()));
    for (const auto peer : peers) {
        writer.WriteUint32(peer.value);
    }
    // FNV-1a over the canonical roster detects different participant lists.
    roster_hash = 14695981039346656037ULL;
    for (const auto byte : writer.Finish().bytes) {
        roster_hash ^= std::to_integer<std::uint8_t>(byte);
        roster_hash *= 1099511628211ULL;
    }
}

void GooseNetwork::RequestReady(std::uint64_t initial_state_hash)
{
    if (initial_hash && *initial_hash != initial_state_hash) {
        throw std::logic_error("GooseNetwork readiness cannot change the initial state.");
    }
    initial_hash = initial_state_hash;
    for (const auto& hash : ready_hashes) {
        if (hash && *hash != initial_state_hash) {
            failure = "Initial state differs between peers.";
        }
    }
}

void GooseNetwork::ReadReady(const svanes::SessionMessage& message)
{
    svanes::MessageReader reader(message.payload);
    const auto count = reader.ReadUint32();
    const auto roster = reader.ReadUint64();
    const auto step = reader.ReadUint64();
    const auto hash = reader.ReadUint64();
    if (reader.Remaining() != 0) {
        throw std::invalid_argument("Goose Ready message has trailing data.");
    }
    if (count != peers.size() || roster != roster_hash || step != GooseStepTics) {
        failure = "Peers disagree on the roster or simulation step.";
        return;
    }
    const auto found = std::find(peers.begin(), peers.end(), message.sender);
    if (found == peers.end() || message.sender == LocalPeer()) {
        throw std::invalid_argument("Goose Ready message has an invalid sender.");
    }
    const auto index = static_cast<std::size_t>(found - peers.begin());
    if ((ready_hashes[index] && *ready_hashes[index] != hash) ||
        (initial_hash && *initial_hash != hash)) {
        failure = "Initial state differs between peers.";
        return;
    }
    ready_hashes[index] = hash;
}

void GooseNetwork::Update()
{
    session->Update();
    svanes::PeerId failed_peer;
    while (session->ReceivePeerFailure(failed_peer)) {
        if (failure.empty()) {
            failure = "Peer " + std::to_string(failed_peer.value) + " stopped acknowledging messages. Restart the session.";
        }
    }
    if (HasFailed()) {
        messages.clear();
        return;
    }

    svanes::SessionMessage message;
    while (messages.size() < 256 && session->Receive(message)) {
        switch (static_cast<GooseMessageType>(message.type)) {
        case GooseMessageType::Ready:
            ReadReady(message);
            break;
        case GooseMessageType::Input:
        case GooseMessageType::StateHash:
            messages.push_back(std::move(message));
            break;
        default:
            throw std::invalid_argument("GooseNetwork received an unknown game message type.");
        }
        if (HasFailed()) {
            messages.clear();
            return;
        }
    }

    if (initial_hash && !ready_sent) {
        svanes::MessageWriter writer;
        writer.WriteUint32(static_cast<std::uint32_t>(peers.size()));
        writer.WriteUint64(roster_hash);
        writer.WriteUint64(GooseStepTics);
        writer.WriteUint64(*initial_hash);
        ready_sent = session->Broadcast(static_cast<svanes::MessageType>(GooseMessageType::Ready), writer.Finish());
    }
}

bool GooseNetwork::Broadcast(GooseMessageType type, const svanes::NetworkMessage& payload)
{
    if (type != GooseMessageType::Input && type != GooseMessageType::StateHash) {
        throw std::invalid_argument("GooseNetwork::Broadcast requires Input or StateHash.");
    }
    return IsReady() && session->Broadcast(static_cast<svanes::MessageType>(type), payload);
}

bool GooseNetwork::Receive(svanes::SessionMessage& message)
{
    if (HasFailed() || messages.empty()) {
        return false;
    }
    message = std::move(messages.front());
    messages.pop_front();
    return true;
}

std::span<const svanes::PeerId> GooseNetwork::Peers() const
{
    return peers;
}

svanes::PeerId GooseNetwork::LocalPeer() const
{
    return session->LocalPeer();
}

bool GooseNetwork::IsReady() const
{
    if (HasFailed() || !ready_sent) {
        return false;
    }
    for (std::size_t index = 0; index < peers.size(); ++index) {
        if (peers[index] != LocalPeer() && !ready_hashes[index]) {
            return false;
        }
    }
    return true;
}

bool GooseNetwork::HasFailed() const
{
    return !failure.empty();
}

std::string GooseNetwork::Status() const
{
    if (HasFailed()) {
        return failure;
    }
    if (IsReady()) {
        return "All peers ready.";
    }
    std::size_t count = ready_sent ? 1 : 0;
    for (const auto& hash : ready_hashes) {
        if (hash) {
            ++count;
        }
    }
    return std::to_string(count) + "/" + std::to_string(peers.size()) +
        " peers ready. " + (initial_hash ? "Waiting for other players." : "Open all windows, then press Enter in each.");
}
