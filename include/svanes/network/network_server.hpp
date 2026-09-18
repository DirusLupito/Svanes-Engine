#pragma once

#include <svanes/network/msg_pipe.hpp>
#include <svanes/network/network_message.hpp>

#include <memory>
#include <vector>

namespace svanes {

class NetworkServer final {
public:
    explicit NetworkServer(std::unique_ptr<MsgPipe> pipe);

    template <typename T> void Broadcast(const T &message) {
        pipe->Send(NetworkMessage::From(message));
    }

    std::vector<NetworkMessage> PollInbound();

private:
    std::unique_ptr<MsgPipe> pipe;
};

} // namespace svanes
