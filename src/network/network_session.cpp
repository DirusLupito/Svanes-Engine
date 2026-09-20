#include <svanes/network/network_session.hpp>

#include <svanes/network/message_serialization.hpp>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace svanes {

namespace {

constexpr std::uint32_t ProtocolMarker = 0x53564e53; // "SVNS" on the wire.
constexpr std::uint16_t ProtocolVersion = 1;
constexpr std::size_t HeaderBytes = 24;

}

NetworkMessage EncodeSessionMessage(SessionId session,
                                    const SessionMessage &message) {
    if (session == 0 || message.sender.value == 0) {
        throw std::invalid_argument("Session envelope: session and sender must be nonzero.");
    }
    if (message.payload.bytes.size() > MaxSessionPayloadBytes) {
        throw std::length_error("Session envelope: payload exceeds the 1176-byte limit.");
    }

    MessageWriter writer;
    writer.WriteUint32(ProtocolMarker);
    writer.WriteUint16(ProtocolVersion);
    writer.WriteUint16(message.type);
    writer.WriteUint64(session);
    writer.WriteUint32(message.sender.value);
    writer.WriteUint32(static_cast<std::uint32_t>(message.payload.bytes.size()));
    writer.WriteBytes(message.payload.bytes);
    return writer.Finish();
}

SessionMessage DecodeSessionMessage(SessionId session,
                                    const NetworkMessage &message) {
    if (message.bytes.size() < HeaderBytes ||
        message.bytes.size() > HeaderBytes + MaxSessionPayloadBytes) {
        throw std::invalid_argument("Session envelope: invalid message size.");
    }
    MessageReader reader(message);
    if (reader.ReadUint32() != ProtocolMarker) {
        throw std::invalid_argument("Session envelope: incorrect protocol marker.");
    }
    if (reader.ReadUint16() != ProtocolVersion) {
        throw std::invalid_argument("Session envelope: unsupported protocol version.");
    }
    SessionMessage result;
    result.type = reader.ReadUint16();
    if (session == 0 || reader.ReadUint64() != session) {
        throw std::invalid_argument("Session envelope: session identifier mismatch.");
    }
    result.sender = PeerId{reader.ReadUint32()};
    if (result.sender.value == 0) {
        throw std::invalid_argument("Session envelope: sender must be nonzero.");
    }
    const auto length = reader.ReadUint32();
    // Require an exact match so neither truncation nor trailing data is accepted.
    if (length != reader.Remaining()) {
        throw std::invalid_argument("Session envelope: payload length mismatch.");
    }
    const auto payload = reader.ReadBytes(length);
    result.payload.bytes.assign(payload.begin(), payload.end());
    return result;
}

NetworkSession::NetworkSession(std::unique_ptr<MsgPipe> pipe,
                               SessionConfiguration configuration)
    : pipe(std::move(pipe)), configuration(std::move(configuration)) {
    if (!this->pipe || this->configuration.session == 0 ||
        this->configuration.local_peer.value == 0) {
        throw std::invalid_argument("NetworkSession requires a pipe and nonzero session/local peer identifiers.");
    }
    const auto connections = this->pipe->Connections();
    const auto &peers = this->configuration.remote_peers;
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
    for (const auto &entry : configuration.remote_peers) {
        if (entry.peer == destination) {
            return pipe->Send(entry.connection, EncodeSessionMessage(
                configuration.session, {configuration.local_peer, type, payload}));
        }
    }
    throw std::invalid_argument("NetworkSession::Send: peer is not in the configured roster.");
}

bool NetworkSession::Broadcast(MessageType type, const NetworkMessage &payload) {
    const auto message = EncodeSessionMessage(
        configuration.session, {configuration.local_peer, type, payload});
    bool accepted = true;
    // Learned transport routes do not automatically become session members.
    for (const auto &entry : configuration.remote_peers) {
        if (!pipe->Send(entry.connection, message)) {
            accepted = false;
        }
    }
    return accepted;
}

bool NetworkSession::Receive(SessionMessage &received) {
    ReceivedMessage incoming;
    if (!pipe->Receive(incoming)) {
        return false;
    }
    const auto found = std::find_if(configuration.remote_peers.begin(),
                                   configuration.remote_peers.end(),
                                   [&](const PeerConnection &entry) {
                                       return entry.connection == incoming.source;
                                   });
    if (found == configuration.remote_peers.end()) {
        throw std::invalid_argument("NetworkSession: message from a connection outside the roster.");
    }
    auto decoded = DecodeSessionMessage(configuration.session, incoming.message);
    // The envelope's claimed sender must agree with the configured source route.
    if (decoded.sender != found->peer) {
        throw std::invalid_argument("NetworkSession: envelope sender does not match its configured connection.");
    }
    received = std::move(decoded);
    return true;
}

}
