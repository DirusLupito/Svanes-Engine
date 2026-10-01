#pragma once

#include <svanes/network/msg_pipe.hpp>
#include <svanes/network/network_message.hpp>

#include <zmq.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace svanes {

/**
 * Concrete implementation of MsgPipe using TCP.
 * Uses ZMQ PAIR sockets to send raw bytes.
 */
class TcpMsgPipe final : public MsgPipe {
public:
    /**
     * Opens a TCP pipe for a client to connect to.
     *
     * @param local_port The local port to listen on, or zero to choose an
     * available port. Port() returns the chosen port.
     * @throws zmq::error_t if the listening pipe cannot be opened.
     */
    explicit TcpMsgPipe(std::uint16_t local_port);

    TcpMsgPipe(const std::string &remote_host, std::uint16_t remote_port);

    /**
     * Finds the pipe's endpoint port so a joining client can be directed to it.
     *
     * @return The local listening port, or the destination port for a
     * connecting pipe.
     * @throws zmq::error_t if the endpoint cannot be queried.
     */
    std::uint16_t Port() const;

    using MsgPipe::Receive;
    using MsgPipe::Send;

    bool Send(ConnectionId destination, const NetworkMessage &message) override;
    bool Receive(ReceivedMessage &received) override;

private:
    zmq::context_t context;
    zmq::socket_t socket;
    bool listening;
};

} // namespace svanes
