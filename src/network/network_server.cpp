#include <svanes/network/network_server.hpp>

#include <utility>

namespace svanes {

NetworkServer::NetworkServer(std::unique_ptr<MsgPipe> pipe)
    : pipe(std::move(pipe)) {}

std::vector<NetworkMessage> NetworkServer::PollInbound() {
    std::vector<NetworkMessage> messages;

    NetworkMessage message;
    while (pipe->Receive(message)) {
        messages.push_back(std::move(message));
    }

    return messages;
}

} // namespace svanes
