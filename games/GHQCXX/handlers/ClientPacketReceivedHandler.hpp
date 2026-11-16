#pragma once

#include "../events/ClientPacketReceivedEvent.hpp"

#include <svanes/events/event_handler_wrapper.hpp>

namespace ghqcxx {

/**
 * Handles gameplay packets received by the client from the server.
 */
class ClientPacketReceivedHandler final
    : public svanes::EventHandlerWrapper<ClientPacketReceivedEvent> {
public:
    /**
     * Creates a handler for the client's incoming gameplay packets.
     */
    ClientPacketReceivedHandler();
};

} // namespace ghqcxx
