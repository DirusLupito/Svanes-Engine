#pragma once

#include <svanes/network/msg_pipe.hpp>
#include <svanes/network/network_message.hpp>

#include <memory>
#include <vector>

namespace svanes {

class NetworkClient final {
public:
    explicit NetworkClient(std::unique_ptr<MsgPipe> pipe);

    ClientId Id() const;

    template <typename T> void Send(const T &message) {
        pipe->Send(NetworkMessage::From(message));
    }

    std::vector<NetworkMessage> PollBroadcast();

private:
    ClientId id;
    std::unique_ptr<MsgPipe> pipe;
};

} // namespace svanes
