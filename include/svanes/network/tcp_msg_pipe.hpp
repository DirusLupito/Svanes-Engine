#pragma once

#include <svanes/network/msg_pipe.hpp>
#include <svanes/network/network_message.hpp>

#include <zmq.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace svanes {

/**
 * Concrete implementation of a MsgPipe using TCP protocol.
 * Uses ZMQ_ROUTER and ZMQ_DEALER socket types
 */
class TcpMsgPipe final : public MsgPipe {
public:
    /**
     * Creates a router socket bound to the local port.
     * 
     * @param local_port The port number to bind the socket to.
     */
    explicit TcpMsgPipe(std::uint16_t local_port);

    /**
     * Creates a dealer socket and connects to a remote host.
     * 
     * @param remote_host The hostname of the remote peer the socket will be bound to.
     * @param remote_port The port number of the remote peer the socket will be bound to.
     */
    TcpMsgPipe(const std::string &remote_host, std::uint16_t remote_port);

    using MsgPipe::Send;
    using MsgPipe::Receive;

    /**
     * Sends to a particular client, or to the server when acting as a client.
     *
     * @param destination The connection to send to.
     * @param message The bytes to send.
     * @return Whether the send was accepted locally, not confirmation of receipt.
     * @throws std::invalid_argument if the connection id is unknown.
     */
    bool Send(ConnectionId destination, const NetworkMessage &message) override;

    /**
     * Receives a message and identifies the client or server that sent it.
     *
     * @param received Receives the source connection id and payload.
     * @return Whether a message was available.
     * @throws std::runtime_error if the message has an invalid frame layout.
     */
    bool Receive(ReceivedMessage &received) override;

private:
    zmq::context_t context;
    zmq::socket_t socket;
    bool listening;
};

} // namespace svanes
