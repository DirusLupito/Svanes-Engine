#pragma once

#include "../GameMessage.hpp"

#include <svanes/events/event.hpp>

#include <string_view>

namespace ghqcxx {

/**
 * Represents a player's request to undo the last action of the current turn.
 */
class UndoMoveEvent final : public svanes::EventOf<UndoMoveEvent> {
public:
    // The event name for undoing a move.
    static constexpr std::string_view EventName = "ghqcxx.undo_move";

    /**
     * Encodes the request for transmission.
     *
     * @return The undo message.
     */
    svanes::NetworkMessage ToMessage() const;

    /**
     * Decodes an undo request from a gameplay message.
     *
     * @param message The received message.
     *
     * @return The decoded event.
     *
     * @throws std::invalid_argument if the message type or length is invalid.
     */
    static UndoMoveEvent FromMessage(const svanes::NetworkMessage &message);
};

} // namespace ghqcxx
