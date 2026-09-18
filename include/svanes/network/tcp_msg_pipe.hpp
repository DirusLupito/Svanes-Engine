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
class TcpMsgPipe final : public MsgPipe {
public:
    /**
     *
     */
    explicit TcpMsgPipe(std::uint16_t local_port);

    /**
     *
     */
    TcpMsgPipe(const std::string &remote_host, std::uint16_t remote_port);

    /**
     *
     */
    bool Send(const NetworkMessage &message) override;

    /**
     *
     */
    bool Receive(NetworkMessage &message) override;

private:
    zmq::context_t context;
    zmq::socket_t socket;
    bool listening;
    std::vector<std::string> peers;
};

} // namespace svanes
