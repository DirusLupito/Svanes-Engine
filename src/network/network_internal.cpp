#include "network_internal.hpp"

namespace svanes::internal {

std::vector<NetworkMessage> NetworkInternal::DrainMessages(zmq::socket_t &socket) {
    std::vector<NetworkMessage> messages;

    while (true) {
        zmq::message_t message;
        const zmq::recv_result_t result = socket.recv(message, zmq::recv_flags::dontwait);
        if (!result.has_value()) {
            break;
        }

        const auto *bytes = static_cast<const std::byte *>(message.data());
        messages.push_back(NetworkMessage{std::vector<std::byte>(bytes, bytes + message.size())});
    }

    return messages;
}

} // namespace svanes::internal
