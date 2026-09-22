#include <svanes/network/network_session.hpp>

#include <svanes/network/message_serialization.hpp>

#include <algorithm>
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
        packet.kind != SessionPacketKind::Acknowledgment) {
        throw std::invalid_argument("Session packet: unknown packet kind.");
    }
    if (packet.kind == SessionPacketKind::Acknowledgment &&
        (packet.type != 0 || !packet.payload.bytes.empty())) {
        throw std::invalid_argument("Session packet: acknowledgments cannot carry a game type or payload.");
    }
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
        if (!CanQueue(peer)) {
            return false;
        }
    }
    for (auto &peer : peers) {
        QueueMessage(peer, type, payload);
    }
    return true;
}

bool NetworkSession::CanQueue(const PeerState &peer) const {
    if (peer.failed) {
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
    if (!duplicate) {
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
        const auto found = std::find_if(configuration.remote_peers.begin(),
                                       configuration.remote_peers.end(),
                                       [&](const PeerConnection &entry) {
                                           return entry.connection == incoming.source;
                                       });
        if (found == configuration.remote_peers.end()) {
            throw std::invalid_argument("NetworkSession: message from a connection outside the roster.");
        }
        auto decoded = DecodeSessionPacket(configuration.session, incoming.message);
        // The envelope's claimed sender must agree with the configured source route.
        if (decoded.sender != found->peer) {
            throw std::invalid_argument("NetworkSession: envelope sender does not match its configured connection.");
        }
        const auto index = static_cast<std::size_t>(found - configuration.remote_peers.begin());
        ProcessPacket(index, std::move(decoded));
    }

    const auto now = std::chrono::steady_clock::now();
    for (std::size_t index = 0; index < peers.size(); ++index) {
        auto &peer = peers[index];
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
            failed_peers.push_back(configuration.remote_peers[index].peer);
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

}
