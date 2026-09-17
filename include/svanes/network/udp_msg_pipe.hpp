#pragma once

#include <svanes/network/msg_pipe.hpp>
#include <svanes/network/network_message.hpp>

#include <zmq.hpp>

#include <cstdint>
#include <string>

namespace svanes {

class UdpMsgPipe final : public MsgPipe {
public:
    UdpMsgPipe(std::uint16_t local_port, const std::string &remote_host,
               std::uint16_t remote_port);

    bool Send(const NetworkMessage &message) override;
    bool Receive(NetworkMessage &message) override;
    void SendEndOfStream() override;
    bool AtEndOfStream() const override;
    bool Failed() const override;

private:
    zmq::context_t context;
    zmq::socket_t radio_socket;
    zmq::socket_t dish_socket;
    bool send_closed = false;
    bool local_misuse_failed = false;
    bool remote_end_of_stream = false;
};

} // namespace svanes
