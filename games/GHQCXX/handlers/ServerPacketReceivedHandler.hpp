#pragma once

#include "../events/ServerPacketReceivedEvent.hpp"

#include <svanes/events/event_handler_wrapper.hpp>

namespace svanes {
class EventManager;
} // namespace svanes

namespace ghqcxx {

/**
 * Handles gameplay packets received by the server from clients.
 */
class ServerPacketReceivedHandler final
    : public svanes::EventHandlerWrapper<ServerPacketReceivedEvent> {
public:
    /**
     * Creates a handler for the server's incoming gameplay packets.
     *
     * @param events The manager used to raise decoded game events. Must
     * outlive the handler.
     */
    explicit ServerPacketReceivedHandler(svanes::EventManager &events);
};

} // namespace ghqcxx
