#include <svanes/events/replay_session.hpp>

#include <svanes/events/event_manager.hpp>
#include <svanes/events/replay_events.hpp>
#include <svanes/input/input_events.hpp>

#include <cmath>
#include <fstream>
#include <limits>

namespace svanes {

ReplaySession::ReplaySession(EventManager &manager, TicCount step_tics,
                             Callbacks callbacks)
    : manager(manager), step_tics(step_tics), callbacks(std::move(callbacks)) {
    if (step_tics == 0 || !this->callbacks.save || !this->callbacks.restore ||
        !this->callbacks.step) {
        throw std::invalid_argument("Replay requires a positive step duration "
                                    "and save/restore/step callbacks.");
    }
}

ReplaySession::State ReplaySession::GetState() const { return state; }

bool ReplaySession::IsReplaying() const {
    return state == State::Playing || state == State::Paused;
}

void ReplaySession::CheckBoundary() const {
    if (manager.IsDeliveringEvents() || step_open || updating) {
        throw std::logic_error("Replay operation requires a simulation "
                               "boundary outside event delivery.");
    }
}

void ReplaySession::RegisterCodec(std::string_view type, Codec codec) {
    CheckBoundary();
    if (state != State::Idle) {
        throw std::logic_error(
            "Cannot register replay events during a session.");
    }
    if (type.empty() || type.starts_with("svanes.replay.")) {
        throw std::invalid_argument("Invalid replay driving event type.");
    }
    if (!codecs.emplace(HashText(type), std::move(codec)).second) {
        throw std::invalid_argument("Replay event type is already registered.");
    }
}

void ReplaySession::RegisterInputEvents() {
    RegisterEvent<KeyInputEvent>(
        [](MessageWriter &writer, const KeyInputEvent &event) {
            writer.WriteUint8(static_cast<std::uint8_t>(event.key));
            writer.WriteBool(event.down);
        },
        [](MessageReader &reader) {
            const auto key = reader.ReadUint8();
            const auto down = reader.ReadBool();
            if (key >= static_cast<std::uint8_t>(Key::Count)) {
                throw std::invalid_argument("Invalid replay key.");
            }
            return std::make_unique<KeyInputEvent>(static_cast<Key>(key), down);
        });
    RegisterEvent<MouseButtonInputEvent>(
        [](MessageWriter &writer, const MouseButtonInputEvent &event) {
            writer.WriteUint8(static_cast<std::uint8_t>(event.button));
            writer.WriteBool(event.down);
        },
        [](MessageReader &reader) {
            const auto button = reader.ReadUint8();
            const auto down = reader.ReadBool();
            if (button >= static_cast<std::uint8_t>(MouseButton::Count)) {
                throw std::invalid_argument("Invalid replay mouse button.");
            }
            return std::make_unique<MouseButtonInputEvent>(
                static_cast<MouseButton>(button), down);
        });
    RegisterEvent<MouseMotionInputEvent>(
        [](MessageWriter &writer, const MouseMotionInputEvent &event) {
            writer.WriteFloat32(event.position.x);
            writer.WriteFloat32(event.position.y);
            writer.WriteFloat32(event.delta.x);
            writer.WriteFloat32(event.delta.y);
        },
        [](MessageReader &reader) {
            const auto x = reader.ReadFloat32();
            const auto y = reader.ReadFloat32();
            const auto dx = reader.ReadFloat32();
            const auto dy = reader.ReadFloat32();
            if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(dx) ||
                !std::isfinite(dy)) {
                throw std::invalid_argument("Invalid replay mouse motion.");
            }
            return std::make_unique<MouseMotionInputEvent>(Vector2D{x, y},
                                                           Vector2D{dx, dy});
        });
}

bool ReplaySession::Observe(const Event &event) {
    // Handling for replay events
    switch (event.GetTypeId()) {
    case EventTypeIdOf<StartRecordingEvent>():
        controls.push_back(
            {Action::Record,
             static_cast<const StartRecordingEvent &>(event).path});
        return true;
    case EventTypeIdOf<StopRecordingEvent>():
        controls.push_back({Action::Save, {}});
        return true;
    case EventTypeIdOf<PlayReplayEvent>():
        controls.push_back(
            {Action::Play, static_cast<const PlayReplayEvent &>(event).path});
        return true;
    case EventTypeIdOf<PauseReplayEvent>():
        controls.push_back({Action::Pause, {}});
        return true;
    case EventTypeIdOf<ResumeReplayEvent>():
        controls.push_back({Action::Resume, {}});
        return true;
    case EventTypeIdOf<StopReplayEvent>():
        controls.push_back({Action::Stop, {}});
        return true;
    }

    // Handling for non replay events
    const auto codec = codecs.find(event.GetTypeId());
    if (codec != codecs.end()) {
        if (IsReplaying() && !injecting) { // Live event during playback, drop
            return false;
        }

        if (state == State::Recording) { // If we are recording right now, try to add it to the log
            if (!step_open) { // The recording window is not open right now, can't record
                throw std::logic_error("Deliver replay driving events "
                                       "between BeginStep and EndStep.");
            }

            MessageWriter writer;
            // Encode this event and write it to our log
            codec->second.encode(writer, event);
            pending_events.push_back({std::string{event.GetEventType()},
                                      event.priority, writer.Finish(),
                                      nullptr});
        }
    }

    return true;
}

void ReplaySession::Reset() {
    CheckBoundary();
    state = State::Idle;
    controls.clear();
    recording_path.clear();
    initial_state.bytes.clear();
    steps.clear();
    pending_events.clear();
    playback_step = 0;
    accumulated_tics = 0;
}

void ReplaySession::ProcessControls() {
    CheckBoundary();
    auto pending = std::move(controls);
    controls.clear();
    updating = true;
    try {
        for (const auto &control : pending) {
            switch (control.action) {
            case Action::Record: // Start recording. Requires a replay file
                                 // path. Captures the initial state and clears
                                 // out pending events
                if (state != State::Idle || control.path.empty()) {
                    throw std::logic_error(
                        "Recording requires an idle session and a file path.");
                }
                initial_state = callbacks.save();
                steps.clear();
                pending_events.clear();
                recording_path = control.path;
                state = State::Recording;
                break;
            case Action::Save: // Don't execute unless the ReplaySession is
                               // currently recording. Otherwise saves the
                               // replay file and begins idling
                if (state != State::Recording) {
                    throw std::logic_error("No replay recording is active.");
                }
                Save();
                state = State::Idle;
                break;
            case Action::Play: // Looks for a replay file at the path and begins playback
                if (state != State::Idle || control.path.empty()) {
                    throw std::logic_error(
                        "Playback requires an idle session and a file path.");
                }
                Load(control.path);
                manager.ClearQueuedEvents();
                callbacks.restore(initial_state.bytes);
                playback_step = 0;
                accumulated_tics = 0;
                state = steps.empty() ? State::Idle : State::Playing;
                break;
            case Action::Pause: // Only executes while playing. Pauses playback. Duh.
                if (state != State::Playing) {
                    throw std::logic_error("Cannot pause an inactive replay.");
                }
                state = State::Paused;
                break;
            case Action::Resume: // Only executes while paused. Pauses playback. Double duh.
                if (state != State::Paused) {
                    throw std::logic_error(
                        "Cannot resume a replay that is not paused.");
                }
                state = State::Playing;
                break;
            case Action::Stop: // Ends the playback
                if (!IsReplaying()) {
                    throw std::logic_error("No replay playback is active.");
                }
                manager.ClearQueuedEvents();
                state = State::Idle;
                accumulated_tics = 0;
                break;
            }
        }
    } catch (...) {
        updating = false;
        throw;
    }
    updating = false;
}

void ReplaySession::BeginStep() {
    ProcessControls();
    if (IsReplaying()) {
        throw std::logic_error(
            "Cannot run a live step during replay playback.");
    }
    step_open = true;
}

void ReplaySession::EndStep() {
    if (manager.IsDeliveringEvents() || !step_open) {
        throw std::logic_error(
            "EndStep requires a completed live simulation step.");
    }
    if (state == State::Recording) {
        steps.push_back(std::move(pending_events));
        pending_events.clear();
    }
    step_open = false;
    ProcessControls();
}

void ReplaySession::Update(TicCount elapsed_tics) {
    ProcessControls();
    // Check that we're in the right state
    if (state != State::Playing) {
        return;
    }
    if (elapsed_tics >
        std::numeric_limits<TicCount>::max() - accumulated_tics) {
        throw std::overflow_error("Replay elapsed time overflow.");
    }
    accumulated_tics += elapsed_tics;
    try {
        while (state == State::Playing && accumulated_tics >= step_tics) {
            updating = true;
            // We update injecting to let the observer know that these events
            // are a part of the playback that needs to be let through to the
            // event handlers.
            for (auto &event : steps[playback_step]) {
                injecting = true;
                manager.TriggerEvent(*event.decoded);
                injecting = false;
            }
            // Advance the playback and prepare for the next step
            callbacks.step(step_tics);
            ++playback_step;
            accumulated_tics -= step_tics;
            updating = false;
            ProcessControls();
            // If we're on the last step, then playback is complete and we move back to idling.
            if (IsReplaying() && playback_step == steps.size()) {
                state = State::Idle;
                accumulated_tics = 0;
            }
        }
    } catch (...) { // If there's some issue, abort playback
        updating = false;
        injecting = false;
        state = State::Idle;
        accumulated_tics = 0;
        throw;
    }
}

void ReplaySession::Save() const {
    MessageWriter writer;
    // Write replay file metadata, as well as the initial state
    writer.WriteUint32(0x53565250);
    writer.WriteUint64(step_tics);
    writer.WriteUint64(initial_state.bytes.size());
    writer.WriteBytes(initial_state.bytes);
    writer.WriteUint64(steps.size());
    // Loop through every step taken in the recording
    for (const auto &step : steps) {
        writer.WriteUint64(step.size());
        // Then loops through each encoded event and write it, along with its metadata
        for (const auto &event : step) {
            writer.WriteText(event.type);
            writer.WriteUint64(event.priority);
            writer.WriteUint64(event.payload.bytes.size());
            writer.WriteBytes(event.payload.bytes);
        }
    }

    // Attempt to save the replay file. If it is too large, or if there is an issue
    // saving it, throw an error
    const auto data = writer.Finish();
    if (data.bytes.size() > static_cast<std::uint64_t>(
                                std::numeric_limits<std::streamsize>::max())) {
        throw std::length_error("Replay is too large to save.");
    }
    std::ofstream file(recording_path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char *>(data.bytes.data()),
               static_cast<std::streamsize>(data.bytes.size()));
    file.close();
    if (!file) {
        throw std::runtime_error("Could not save replay: " +
                                 recording_path.string());
    }
}

void ReplaySession::Load(const std::filesystem::path &path) {
    // Try to load the replay file, throw an error if there's an issue doing so
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Could not open replay: " + path.string());
    }
    const auto size = file.tellg();
    if (size < 0 ||
        static_cast<std::uint64_t>(size) >
            std::numeric_limits<std::size_t>::max() ||
        static_cast<std::uint64_t>(size) >
            static_cast<std::uint64_t>(
                std::numeric_limits<std::streamsize>::max())) {
        throw std::length_error("Replay file size is invalid.");
    }
    NetworkMessage data;
    data.bytes.resize(static_cast<std::size_t>(size));
    file.seekg(0);
    file.read(reinterpret_cast<char *>(data.bytes.data()),
              static_cast<std::streamsize>(data.bytes.size()));
    if (!file) {
        throw std::runtime_error("Could not read replay: " + path.string());
    }
    MessageReader reader(data);
    // Check the signature to make sure that this is one of our engine's replay
    // files
    if (reader.ReadUint32() != 0x53565250) {
        throw std::invalid_argument("Invalid replay file signature.");
    }
    // Playback needs the same step duration used during recording.
    const auto duration = reader.ReadUint64();
    if (duration != step_tics) {
        throw std::invalid_argument(
            "Replay simulation step duration does not match.");
    }
    const auto read_bytes = [&reader]() {
        const auto count = reader.ReadUint64();
        if (count > reader.Remaining()) {
            throw std::invalid_argument("Truncated replay payload.");
        }
        return reader.ReadBytes(static_cast<std::size_t>(count));
    };
    const auto saved = read_bytes();
    // Get the starting state from the replay file
    NetworkMessage loaded_state{
        std::vector<std::byte>(saved.begin(), saved.end())};
    const auto count = reader.ReadUint64();
    if (count > reader.Remaining() / 8) {
        throw std::invalid_argument("Invalid replay step count.");
    }
    // Read in all of the steps
    std::vector<Step> loaded_steps;
    loaded_steps.reserve(static_cast<std::size_t>(count));
    for (std::uint64_t index = 0; index < count; ++index) {
        const auto event_count = reader.ReadUint64();
        if (event_count > reader.Remaining() / 18) {
            throw std::invalid_argument("Invalid replay event count.");
        }
        auto &step = loaded_steps.emplace_back();
        step.reserve(static_cast<std::size_t>(event_count));
        // Loop through all the steps that were just read in.
        for (std::uint64_t entry = 0; entry < event_count; ++entry) {
            auto type = reader.ReadText();
            const auto type_id = HashText(type);
            const auto codec = codecs.find(type_id);
            // First we have to check that this event type is registered. If this throws an error,
            // that means that either the replay file is from a different game/version, or that it
            // is being loaded in some different context, where the event was not correctly registered.
            if (codec == codecs.end()) {
                throw std::invalid_argument("Unregistered replay event: " +
                                            type);
            }
            const auto priority = reader.ReadUint64();
            MessageReader payload(read_bytes());
            // If we get here, that means that this event type was registered and we can now decode it.
            auto event = codec->second.decode(payload);
            // If this throws it means someone somewhere goofed. The event was either not encoded properly
            // or not decoded properly.
            if (!event || event->GetTypeId() != type_id ||
                payload.Remaining() != 0) {
                throw std::invalid_argument("Invalid replay event payload: " +
                                            type);
            }
            // If we've gotten through all of that, it means we can finish piecing this event back together
            // and store it as a part of this playback step.
            event->priority = priority;
            event->Handled = false;
            step.push_back({std::move(type), priority, {}, std::move(event)});
        }
    }
    // Leftover data at the end of your replay file? No bueno! That means something was written or read incorrectly!
    if (reader.Remaining() != 0) {
        throw std::invalid_argument("Unexpected trailing replay data.");
    }
    // Once we've read through the whole replay file, we can store the
    // reconstructed data for later playback.
    initial_state = std::move(loaded_state);
    steps = std::move(loaded_steps);
}

} // namespace svanes
