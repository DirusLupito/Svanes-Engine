#pragma once

#include <svanes/events/event.hpp>
#include <svanes/network/network_message.hpp>

#include <string_view>

namespace ghqcxx {

/**
 * Reports a gameplay packet received from the server so client systems can
 * respond to its contents.
 */
class ClientPacketReceivedEvent final
    : public svanes::EventOf<ClientPacketReceivedEvent> {
public:
    // The unique name identifying packets received by the client.
    static constexpr std::string_view EventName =
        "ghqcxx.client.packet_received";

    /**
     * Constructs an event containing a message received from the server.
     *
     * @param message The received gameplay message.
     */
    explicit ClientPacketReceivedEvent(svanes::NetworkMessage message);

    // Owns the received bytes independently of the network's message buffer.
    svanes::NetworkMessage message;
};

} // namespace ghqcxx
