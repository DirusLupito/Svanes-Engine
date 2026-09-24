#pragma once

#include <svanes/network/msg_pipe.hpp>
#include <svanes/network/network_message.hpp>

#include <zmq.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace svanes {

/**
 * Concrete implementation of a MsgPipe using UDP protocol.
 * Uses ZMQ_DGRAM socket to send raw packets
 */
class UdpMsgPipe final : public MsgPipe {
public:
    /**
     * Constructor for a UDP message pipe with no known peers.
     * Remote host information can be learned upon accepting a message with Receive().
     * 
     * @param local_port The local port that the socket will be bound to.
     */
    explicit UdpMsgPipe(std::uint16_t local_port);

    /**
     * Constructor for a UDP message pipe with a destination peer.
     * 
     * @param local_port The local port that the socket will be bound to.
     * @param remote_host The hostname of the remote peer the socket will be bound to.
     * @param remote_port The port number of the remote peer the socket will be bound to
     */
    UdpMsgPipe(std::uint16_t local_port, const std::string &remote_host,
               std::uint16_t remote_port);

    using MsgPipe::Send;
    using MsgPipe::Receive;

    /**
     * Registers a remote address without contacting it. Reuses known routes.
     *
     * @param host The numeric IPv4 address to register.
     * @param port The destination port, which must be nonzero.
     * @return The connection id for the normalized address and port.
     * @throws std::invalid_argument if the address or port is invalid.
     */
    ConnectionId AddRemote(const std::string &host, std::uint16_t port);

    /**
     * Sends one datagram to the given connection id.
     *
     * @param destination The connection to send to.
     * @param message The datagram payload.
     * @return Whether the send was accepted locally, not whether it arrived.
     * @throws std::invalid_argument if the connection id is unknown.
     * @throws std::length_error if the payload exceeds 65507 bytes.
     */
    bool Send(ConnectionId destination, const NetworkMessage &message) override;

    /**
     * Receives a datagram and records its source address as a connection.
     *
     * @param received Receives the source connection id and payload.
     * @return Whether a datagram was available.
     * @throws std::runtime_error if an address arrives without its payload.
     */
    bool Receive(ReceivedMessage &received) override;

private:
    zmq::context_t context;
    zmq::socket_t socket;
};

} // namespace svanes
