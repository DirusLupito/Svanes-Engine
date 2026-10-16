#include <svanes/network/network_message.hpp>

#include <utility>

namespace svanes {

NetworkMessage::NetworkMessage() = default;

NetworkMessage::NetworkMessage(std::vector<std::byte> bytes)
    : bytes(std::move(bytes)) {}

} // namespace svanes
