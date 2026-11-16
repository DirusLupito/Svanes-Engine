#pragma once

#include "../events/ServerPacketReceivedEvent.hpp"

#include <svanes/events/event_handler_wrapper.hpp>

namespace ghqcxx {

/**
 * Handles gameplay packets received by the server from clients.
 */
class ServerPacketReceivedHandler final
    : public svanes::EventHandlerWrapper<ServerPacketReceivedEvent> {
public:
    /**
     * Creates a handler for the server's incoming gameplay packets.
     */
    ServerPacketReceivedHandler();
};

} // namespace ghqcxx
