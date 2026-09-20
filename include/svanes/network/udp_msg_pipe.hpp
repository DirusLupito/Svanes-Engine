#pragma once

#include <svanes/network/msg_pipe.hpp>
#include <svanes/network/network_message.hpp>

#include <zmq.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace svanes {

/**
 *
 */
class UdpMsgPipe final : public MsgPipe {
public:
    /**
     *
     */
    explicit UdpMsgPipe(std::uint16_t local_port);

    /**
     *
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
