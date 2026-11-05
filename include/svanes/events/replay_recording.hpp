#pragma once

#include <svanes/events/event.hpp>
#include <svanes/network/network_message.hpp>
#include <svanes/timeline_system.hpp>

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace svanes {

/**
 * Stores a replay's starting state and ordered event payloads. Encodes and
 * decodes the replay file format.
 */
class ReplayRecording final {
public:
    /**
     * An event saved during recording.
     *
     * FIELDS:
     * - type: The event type's name.
     * - priority: The event's original dispatch priority.
     * - payload: The encoded event payload.
     */
    struct RecordedEvent {
        std::string type;
        EventPriority priority;
        NetworkMessage payload;
    };

    // The replayable events from one simulation step, in delivery order.
    using Step = std::vector<RecordedEvent>;

    /**
     * Encodes the starting state and completed simulation steps in the replay
     * file format.
     *
     * @return The bytes to write to a replay file.
     *
     * @throws std::length_error if an event type name exceeds 65535 bytes.
     */
    std::vector<std::byte> Write() const;

    /**
     * Reads the starting state and event payloads from the replay file format.
     *
     * @param bytes The complete contents of a replay file.
     *
     * @return The recording described by the file, owning its starting state
     * and payload bytes.
     *
     * @throws std::invalid_argument if the format or counts are invalid, or
     * bytes are missing or left over.
     */
    static ReplayRecording Read(std::span<const std::byte> bytes);

    // The recorded simulation step duration, in unscaled source tics.
    TicCount step_tics = 0;

    // The serialized game state captured when recording starts.
    NetworkMessage initial_state;

    // The completed simulation steps, including steps with no recorded events.
    std::vector<Step> steps;
};

} // namespace svanes
