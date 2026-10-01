#include <svanes/network/network_client.hpp>
#include <svanes/network/tcp_connection.hpp>

#include <utility>

namespace svanes {

NetworkClient::NetworkClient(std::unique_ptr<MsgPipe> pipe)
    : id(GenerateClientId()), pipe(std::move(pipe)) {}

NetworkClient::NetworkClient(std::string host, std::uint16_t joining_port)
    : id(0), connector(std::make_unique<TcpClientConnector>(std::move(host),
                                                            joining_port)) {}

NetworkClient::~NetworkClient() = default;
NetworkClient::NetworkClient(NetworkClient &&) noexcept = default;
NetworkClient &NetworkClient::operator=(NetworkClient &&) noexcept = default;

ClientId NetworkClient::Id() const { return id; }

bool NetworkClient::IsConnected() const { return pipe != nullptr; }

void NetworkClient::Send(const NetworkMessage &message) {
    if (!pipe) {
        throw std::logic_error("Cannot send before the client is connected.");
    }
    pipe->Send(message);
}

std::vector<NetworkMessage> NetworkClient::PollBroadcast() {
    std::vector<NetworkMessage> messages;

    // Complete the pending join and set the assigned gameplay connection.
    if (connector) {
        auto connection = connector->Poll();
        if (!connection) {
            return messages;
        }
        id = connection->id;
        pipe = std::move(connection->pipe);
        connector.reset();
    }

    NetworkMessage message;
    while (pipe->Receive(message)) {
        messages.push_back(std::move(message));
    }

    return messages;
}

} // namespace svanes
