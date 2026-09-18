#include <svanes/network/network_client.hpp>

#include <utility>

namespace svanes {

NetworkClient::NetworkClient(std::unique_ptr<MsgPipe> pipe)
    : id(GenerateClientId()), pipe(std::move(pipe)) {}

ClientId NetworkClient::Id() const { return id; }

std::vector<NetworkMessage> NetworkClient::PollBroadcast() {
    std::vector<NetworkMessage> messages;

    NetworkMessage message;
    while (pipe->Receive(message)) {
        messages.push_back(std::move(message));
    }

    return messages;
}

} // namespace svanes
