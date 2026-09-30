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
    explicit TcpMsgPipe(std::uint16_t local_port);
    TcpMsgPipe(const std::string &remote_host, std::uint16_t remote_port);

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
