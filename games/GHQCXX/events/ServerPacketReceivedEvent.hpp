#pragma once

#include <svanes/events/event.hpp>
#include <svanes/network/clientserver/network_server.hpp>
#include <svanes/network/network_message.hpp>

#include <string_view>

namespace ghqcxx {

/**
 * Reports a gameplay packet received by the server, identifying the client
 * that sent it so game systems can process the message and respond.
 */
class ServerPacketReceivedEvent final
    : public svanes::EventOf<ServerPacketReceivedEvent> {
public:
    // The unique name identifying packets received by the server.
    static constexpr std::string_view EventName =
        "ghqcxx.server.packet_received";

    /**
     * Constructs an event containing a client's message and identity.
     *
     * @param client The ID of the client that sent the message.
     * @param message The received gameplay message.
     */
    ServerPacketReceivedEvent(svanes::ClientId client,
                              svanes::NetworkMessage message);

    // The sender's ID, used to address replies to this client.
    svanes::ClientId client;

    // Owns the received bytes independently of the network's message buffer.
    svanes::NetworkMessage message;
};

} // namespace ghqcxx
