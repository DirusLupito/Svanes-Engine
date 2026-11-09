#include <svanes/events/replay_recording.hpp>

#include <svanes/network/message_serialization.hpp>

#include <cstdint>
#include <stdexcept>
#include <utility>

namespace svanes {

constexpr std::uint32_t kReplayFileSignature = 0x53565250;

std::vector<std::byte> ReplayRecording::Write() const {
    MessageWriter writer;

    // Write replay file metadata, as well as the initial state

    writer.WriteUint32(kReplayFileSignature);
    writer.WriteUint64(step_tics);
    writer.WriteUint64(initial_state.bytes.size());
    writer.WriteBytes(initial_state.bytes);
    writer.WriteUint64(steps.size());

    // Loop through every step taken in the recording
    for (const auto &step : steps) {
        writer.WriteUint64(step.size());

        // Then loops through each encoded event and write it, along with its
        // metadata
        for (const auto &event : step) {
            writer.WriteText(event.type);
            writer.WriteUint64(event.priority);
            writer.WriteUint64(event.payload.bytes.size());
            writer.WriteBytes(event.payload.bytes);
        }
    }
    return writer.Finish().bytes;
}

ReplayRecording ReplayRecording::Read(std::span<const std::byte> bytes) {
    MessageReader reader(bytes);
    // Check the signature to make sure that this is one of our engine's replay
    // files
    if (reader.ReadUint32() != kReplayFileSignature) {
        throw std::invalid_argument("Invalid replay file signature.");
    }

    ReplayRecording recording;
    recording.step_tics = reader.ReadUint64();

    const auto read_bytes = [&reader]() {
        const auto count = reader.ReadUint64();
        if (count > reader.Remaining()) {
            throw std::invalid_argument("Truncated replay payload.");
        }
        return reader.ReadBytes(static_cast<std::size_t>(count));
    };

    const auto saved = read_bytes();

    // Get the starting state from the replay file
    recording.initial_state.bytes.assign(saved.begin(), saved.end());

    const auto count = reader.ReadUint64();
    if (count > reader.Remaining() / 8) {
        throw std::invalid_argument("Invalid replay step count.");
    }

    // Read in all of the steps
    recording.steps.reserve(static_cast<std::size_t>(count));
    for (std::uint64_t index = 0; index < count; ++index) {
        const auto event_count = reader.ReadUint64();
        if (event_count > reader.Remaining() / 18) {
            throw std::invalid_argument("Invalid replay event count.");
        }

        auto &step = recording.steps.emplace_back();
        step.reserve(static_cast<std::size_t>(event_count));

        // Loop through all the events in this step.
        for (std::uint64_t entry = 0; entry < event_count; ++entry) {
            auto type = reader.ReadText();
            const auto priority = reader.ReadUint64();
            const auto payload = read_bytes();
            step.push_back({std::move(type), priority,
                            NetworkMessage{std::vector<std::byte>(
                                payload.begin(), payload.end())}});
        }
    }
    // Leftover data at the end of your replay file? No bueno! That means
    // something was written or read incorrectly!
    if (reader.Remaining() != 0) {
        throw std::invalid_argument("Unexpected trailing replay data.");
    }

    return recording;
}

} // namespace svanes
