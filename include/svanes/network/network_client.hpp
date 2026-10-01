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

    /**
     * Sends an already encoded message through the client's connection.
     *
     * @param message The encoded content to send to the server.
     */
    void Send(const NetworkMessage &message);

    template <typename T> void Send(const T &message) {
        pipe->Send(NetworkMessage::From(message));
    }

    std::vector<NetworkMessage> PollBroadcast();

private:
    ClientId id;
    std::unique_ptr<MsgPipe> pipe;
};

} // namespace svanes
