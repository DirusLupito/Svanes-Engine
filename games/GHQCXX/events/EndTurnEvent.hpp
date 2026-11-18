#pragma once

#include "../GameMessage.hpp"

#include <svanes/events/event.hpp>

#include <string_view>

namespace ghqcxx {

/**
 * Represents the end of the active team's turn.
 */
class EndTurnEvent final : public svanes::EventOf<EndTurnEvent> {
public:
    // The event name for ending a turn.
    static constexpr std::string_view EventName = "ghqcxx.end_turn";

    /**
     * Encodes the event for transmission.
     *
     * @return The end-turn message.
     */
    svanes::NetworkMessage ToMessage() const;

    /**
     * Decodes an end-turn event from a gameplay message.
     *
     * @param message The received message.
     * @return The decoded event.
     * @throws std::invalid_argument if the message type or length is invalid.
     */
    static EndTurnEvent FromMessage(const svanes::NetworkMessage &message);
};

} // namespace ghqcxx
