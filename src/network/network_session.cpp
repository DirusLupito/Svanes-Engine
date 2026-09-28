#include <svanes/network/network_session.hpp>

#include <svanes/network/message_serialization.hpp>

#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>

namespace svanes {

namespace {

constexpr std::uint32_t ProtocolMarker = 0x53564e53; // "SVNS" on the wire.
constexpr std::size_t HeaderBytes = 31;

/**
 * Checks the ids and fields shared by outgoing and incoming session packets.
 * @param session The session id the packet belongs to.
 * @param packet The decoded fields to validate.
 * @throws std::invalid_argument for invalid ids, kinds, or acknowledgment contents.
 */
void ValidatePacket(SessionId session, const SessionPacket &packet) {
    if (session == 0 || packet.sender.value == 0 || packet.message_id == 0) {
        throw std::invalid_argument("Session packet: session, sender, and message ids must be nonzero.");
    }
    if (packet.kind != SessionPacketKind::Data &&
        packet.kind != SessionPacketKind::Acknowledgment &&
        packet.kind != SessionPacketKind::Contact) {
        throw std::invalid_argument("Session packet: unknown packet kind.");
    }
    if (packet.kind == SessionPacketKind::Acknowledgment &&
        (packet.type != 0 || !packet.payload.bytes.empty())) {
        throw std::invalid_argument("Session packet: acknowledgments cannot carry a game type or payload.");
    }
}

/**
 * Recognizes a well-framed session packet that belongs to a different session.
 * @param session The local session id.
 * @param message The complete received bytes.
 * @return Whether the protocol marker matches but the session id differs.
 */
bool BelongsToOtherSession(SessionId session, const NetworkMessage &message) {
    if (message.bytes.size() < HeaderBytes) {
        return false;
    }
    MessageReader reader(message);
    if (reader.ReadUint32() != ProtocolMarker) {
        return false;
    }
    reader.ReadUint16();
    return reader.ReadUint64() != session;
}

}

NetworkMessage EncodeSessionPacket(SessionId session,
                                   const SessionPacket &packet) {
    ValidatePacket(session, packet);
    if (packet.payload.bytes.size() > MaxSessionPayloadBytes) {
        throw std::length_error("Session packet: payload exceeds MaxSessionPayloadBytes.");
    }

    MessageWriter writer;
    writer.WriteUint32(ProtocolMarker);
    writer.WriteUint16(packet.type);
    writer.WriteUint64(session);
    writer.WriteUint32(packet.sender.value);
    writer.WriteUint8(static_cast<std::uint8_t>(packet.kind));
    writer.WriteUint64(packet.message_id);
    writer.WriteUint32(static_cast<std::uint32_t>(packet.payload.bytes.size()));
    writer.WriteBytes(packet.payload.bytes);
    return writer.Finish();
}

SessionPacket DecodeSessionPacket(SessionId session,
                                  const NetworkMessage &message) {
    if (message.bytes.size() < HeaderBytes ||
        message.bytes.size() > HeaderBytes + MaxSessionPayloadBytes) {
        throw std::invalid_argument("Session envelope: invalid message size.");
    }
    MessageReader reader(message);
    if (reader.ReadUint32() != ProtocolMarker) {
        throw std::invalid_argument("Session envelope: incorrect protocol marker.");
    }
    SessionPacket result;
    result.type = reader.ReadUint16();
    if (session == 0 || reader.ReadUint64() != session) {
        throw std::invalid_argument("Session envelope: session identifier mismatch.");
    }
    result.sender = PeerId{reader.ReadUint32()};
    result.kind = static_cast<SessionPacketKind>(reader.ReadUint8());
    result.message_id = reader.ReadUint64();
    const auto length = reader.ReadUint32();
    // Require an exact match so neither truncation nor trailing data is accepted.
    if (length != reader.Remaining()) {
        throw std::invalid_argument("Session envelope: payload length mismatch.");
    }
    const auto payload = reader.ReadBytes(length);
    result.payload.bytes.assign(payload.begin(), payload.end());
    ValidatePacket(session, result);
    return result;
}

NetworkSession::NetworkSession(std::unique_ptr<MsgPipe> pipe,
                               SessionConfiguration configuration,
                               ReliabilitySettings settings)
    : pipe(std::move(pipe)), configuration(std::move(configuration)), settings(settings) {
    if (!this->pipe || this->configuration.session == 0 ||
        this->configuration.local_peer.value == 0) {
        throw std::invalid_argument("NetworkSession requires a pipe and nonzero session/local peer identifiers.");
    }
    if (settings.retry_interval.count() <= 0 ||
        settings.delivery_timeout < settings.retry_interval ||
        settings.max_pending_per_peer == 0 || settings.max_incoming_messages == 0 ||
        settings.max_packets_per_update == 0) {
        throw std::invalid_argument("NetworkSession requires positive retry intervals and queue limits, with delivery timeout at least the retry interval.");
    }
    const auto connections = this->pipe->Connections();
    const auto &peers = this->configuration.remote_peers;
    this->peers.resize(peers.size());
    // Each remote peer must map to one distinct connection owned by this pipe.
    for (std::size_t index = 0; index < peers.size(); ++index) {
        const auto &entry = peers[index];
        if (entry.peer.value == 0 || entry.peer == this->configuration.local_peer ||
            std::find(connections.begin(), connections.end(), entry.connection) == connections.end()) {
            throw std::invalid_argument("NetworkSession: invalid remote peer or connection.");
        }
        for (std::size_t previous = 0; previous < index; ++previous) {
            if (peers[previous].peer == entry.peer ||
                peers[previous].connection == entry.connection) {
                throw std::invalid_argument("NetworkSession: duplicate peer or connection in roster.");
            }
        }
    }
}

PeerId NetworkSession::LocalPeer() const { return configuration.local_peer; }

std::span<const PeerConnection> NetworkSession::RemotePeers() const {
    return configuration.remote_peers;
}

bool NetworkSession::Send(PeerId destination, MessageType type,
                          const NetworkMessage &payload) {
    if (payload.bytes.size() > MaxSessionPayloadBytes) {
        throw std::length_error("NetworkSession::Send: payload exceeds MaxSessionPayloadBytes.");
    }
    for (std::size_t index = 0; index < configuration.remote_peers.size(); ++index) {
        const auto &entry = configuration.remote_peers[index];
        if (entry.peer == destination) {
            auto &peer = peers[index];
            if (!CanQueue(peer)) {
                return false;
            }
            QueueMessage(peer, type, payload);
            return true;
        }
    }
    throw std::invalid_argument("NetworkSession::Send: peer is not in the configured roster.");
}

bool NetworkSession::Broadcast(MessageType type, const NetworkMessage &payload) {
    if (payload.bytes.size() > MaxSessionPayloadBytes) {
        throw std::length_error("NetworkSession::Broadcast: payload exceeds MaxSessionPayloadBytes.");
    }
    // Check every destination before accepting any part of the broadcast.
    for (const auto &peer : peers) {
        if (!peer.retired && !CanQueue(peer)) {
            return false;
        }
    }
    for (auto &peer : peers) {
        if (!peer.retired) {
            QueueMessage(peer, type, payload);
        }
    }
    return true;
}

bool NetworkSession::CanQueue(const PeerState &peer) const {
    if (peer.failed || peer.retired) {
        return false;
    }
    if (peer.next_message_id == 0) {
        throw std::overflow_error("NetworkSession: message ids exhausted for this peer.");
    }
    if (peer.pending.size() >= settings.max_pending_per_peer) {
        return false;
    }
    // Acknowledging later messages must not let a missing early id fall out of the window.
    return peer.pending.empty() ||
        peer.next_message_id - peer.pending.begin()->first < settings.max_pending_per_peer;
}

void NetworkSession::QueueMessage(PeerState &peer, MessageType type,
                                  const NetworkMessage &payload) {
    auto packet = EncodeSessionPacket(configuration.session,
        {SessionPacketKind::Data, configuration.local_peer, peer.next_message_id, type, payload});
    peer.pending.emplace(peer.next_message_id, PendingMessage{std::move(packet), std::nullopt, {}});
    peer.next_message_id = peer.next_message_id == std::numeric_limits<MessageId>::max()
        ? 0 : peer.next_message_id + 1;
}

void NetworkSession::ProcessPacket(std::size_t index, SessionPacket packet) {
    auto &peer = peers[index];
    if (peer.failed) {
        return;
    }
    if (packet.kind == SessionPacketKind::Acknowledgment) {
        const auto pending = peer.pending.find(packet.message_id);
        if (pending != peer.pending.end() && pending->second.first_attempt) {
            peer.pending.erase(pending);
        }
        return;
    }

    const bool duplicate = packet.message_id <= peer.received_through ||
        peer.received_ahead.contains(packet.message_id);
    if (!duplicate && !peer.retired) {
        if (packet.message_id - peer.received_through > settings.max_pending_per_peer ||
            incoming_messages.size() >= settings.max_incoming_messages) {
            // Leave unaccepted data unacknowledged so the sender retains it for retry.
            return;
        }
        incoming_messages.push_back({packet.sender, packet.type, std::move(packet.payload)});
        peer.received_ahead.insert(packet.message_id);
        while (peer.received_through != std::numeric_limits<MessageId>::max() &&
               peer.received_ahead.erase(peer.received_through + 1) != 0) {
            ++peer.received_through;
        }
    }

    const auto acknowledgment = EncodeSessionPacket(configuration.session,
        {SessionPacketKind::Acknowledgment, configuration.local_peer, packet.message_id, 0, {}});
    // If this acknowledgment cannot be sent, a repeated data packet triggers another.
    static_cast<void>(pipe->Send(configuration.remote_peers[index].connection, acknowledgment));
}

void NetworkSession::Update() {
    ReceivedMessage incoming;
    for (std::uint32_t count = 0; count < settings.max_packets_per_update; ++count) {
        if (!pipe->Receive(incoming)) {
            break;
        }
        if (BelongsToOtherSession(configuration.session, incoming.message)) {
            if (wrong_session_connections.insert(incoming.source.value).second) {
                std::cerr << "NetworkSession: dropping packets from connection "
                          << incoming.source.value << ", which uses a different session id.\n";
            }
            continue;
        }
        const auto found = FindConnection(incoming.source);
        auto decoded = DecodeSessionPacket(configuration.session, incoming.message);
        if (!found || decoded.kind == SessionPacketKind::Contact) {
            if (decoded.kind == SessionPacketKind::Acknowledgment) {
                throw std::invalid_argument("NetworkSession: acknowledgment from a connection outside the roster.");
            }
            if (strangers.size() < settings.max_incoming_messages) {
                strangers.push_back({incoming.source,
                    {decoded.sender, decoded.type, std::move(decoded.payload)}});
            }
            continue;
        }
        // The envelope's claimed sender must agree with the configured source route.
        if (decoded.sender != configuration.remote_peers[*found].peer) {
            throw std::invalid_argument("NetworkSession: envelope sender does not match its configured connection.");
        }
        ProcessPacket(*found, std::move(decoded));
    }

    const auto now = std::chrono::steady_clock::now();
    for (std::size_t index = 0; index < peers.size(); ++index) {
        auto &peer = peers[index];
        if (peer.retired && !peer.released && peer.pending.empty() &&
            now - peer.retired_at >= settings.delivery_timeout) {
            peer.released = true;
            peer.received_ahead.clear();
        }
        if (peer.failed) {
            continue;
        }
        const bool timed_out = std::any_of(peer.pending.begin(), peer.pending.end(),
            [&](const auto &entry) {
                const auto &message = entry.second;
                return message.first_attempt &&
                    std::chrono::duration_cast<std::chrono::milliseconds>(now - *message.first_attempt)
                        >= settings.delivery_timeout;
            });
        if (timed_out) {
            if (!peer.retired) {
                failed_peers.push_back(configuration.remote_peers[index].peer);
            }
            peer.failed = true;
            peer.pending.clear();
            peer.received_ahead.clear();
            continue;
        }
        for (auto &[id, message] : peer.pending) {
            if (message.first_attempt &&
                std::chrono::duration_cast<std::chrono::milliseconds>(now - message.last_attempt)
                    < settings.retry_interval) {
                continue;
            }
            if (!message.first_attempt) {
                message.first_attempt = now;
            }
            message.last_attempt = now;
            // Local backpressure leaves the packet pending just like a lost datagram.
            static_cast<void>(pipe->Send(configuration.remote_peers[index].connection, message.packet));
        }
    }
}

bool NetworkSession::Receive(SessionMessage &received) {
    if (incoming_messages.empty()) {
        return false;
    }
    received = std::move(incoming_messages.front());
    incoming_messages.pop_front();
    return true;
}

bool NetworkSession::ReceivePeerFailure(PeerId &peer) {
    if (failed_peers.empty()) {
        return false;
    }
    peer = failed_peers.front();
    failed_peers.pop_front();
    return true;
}

bool NetworkSession::ReceiveStranger(StrangerMessage &stranger) {
    if (strangers.empty()) {
        return false;
    }
    stranger = std::move(strangers.front());
    strangers.pop_front();
    return true;
}

void NetworkSession::AddPeer(PeerConnection peer) {
    const auto connections = pipe->Connections();
    if (peer.peer.value == 0 || peer.peer == configuration.local_peer ||
        std::find(connections.begin(), connections.end(), peer.connection) == connections.end()) {
        throw std::invalid_argument("NetworkSession::AddPeer: invalid peer or connection.");
    }
    for (const auto &entry : configuration.remote_peers) {
        if (entry.peer == peer.peer) {
            throw std::invalid_argument("NetworkSession::AddPeer: peer id already used in this session.");
        }
    }
    if (const auto holder = FindConnection(peer.connection)) {
        if (!peers[*holder].retired) {
            throw std::invalid_argument("NetworkSession::AddPeer: connection held by an active peer.");
        }
        peers[*holder].released = true;
        peers[*holder].pending.clear();
        peers[*holder].received_ahead.clear();
    }
    configuration.remote_peers.push_back(peer);
    peers.emplace_back();
    std::erase_if(strangers, [&](const auto &stranger) { return stranger.connection == peer.connection; });
}

std::optional<PeerId> NetworkSession::ActivePeerAt(ConnectionId connection) const {
    const auto found = FindConnection(connection);
    if (!found || peers[*found].retired) {
        return std::nullopt;
    }
    return configuration.remote_peers[*found].peer;
}

bool NetworkSession::SendStranger(ConnectionId connection, MessageType type,
                                  const NetworkMessage &payload) {
    return pipe->Send(connection, EncodeSessionPacket(configuration.session,
        {SessionPacketKind::Contact, configuration.local_peer, 1, type, payload}));
}

std::optional<std::size_t> NetworkSession::FindConnection(ConnectionId connection) const {
    for (std::size_t index = 0; index < configuration.remote_peers.size(); ++index) {
        if (configuration.remote_peers[index].connection == connection && !peers[index].released) {
            return index;
        }
    }
    return std::nullopt;
}

void NetworkSession::RetirePeer(PeerId id) {
    for (std::size_t index = 0; index < configuration.remote_peers.size(); ++index) {
        if (configuration.remote_peers[index].peer == id) {
            if (peers[index].retired) {
                return;
            }
            peers[index].retired = true;
            peers[index].retired_at = std::chrono::steady_clock::now();
            std::erase_if(incoming_messages, [&](const auto &message) { return message.sender == id; });
            std::erase(failed_peers, id);
            return;
        }
    }
    throw std::invalid_argument("NetworkSession::RetirePeer: unknown peer.");
}

bool NetworkSession::OutgoingDrained() const {
    return std::all_of(peers.begin(), peers.end(), [](const auto &peer) { return peer.pending.empty(); });
}

}
