#include <svanes/events/replay_session.hpp>

#include <svanes/events/event_manager.hpp>
#include <svanes/events/replay_events.hpp>
#include <svanes/utility/file_io.hpp>

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

void ReplaySession::Play(ReplayRecording incoming_recording) {
    CheckBoundary();
    if (state != State::Idle) {
        throw std::logic_error("Playback requires an idle session.");
    }

    updating = true;
    try {
        Load(std::move(incoming_recording));
        StartPlayback();
    } catch (...) {
        updating = false;
        throw;
    }
    updating = false;
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
                                      event.priority, writer.Finish()});
        }
    }

    return true;
}

void ReplaySession::Reset() {
    CheckBoundary();
    state = State::Idle;
    controls.clear();
    recording_path.clear();
    recording = ReplayRecording{};
    playback_steps.clear();
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
                recording.initial_state = callbacks.save();
                recording.step_tics = step_tics;
                recording.steps.clear();
                playback_steps.clear();
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
                StartPlayback();
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
        recording.steps.push_back(std::move(pending_events));
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
            for (auto &event : playback_steps[playback_step]) {
                injecting = true;
                manager.TriggerEvent(*event);
                injecting = false;
            }
            // Advance the playback and prepare for the next step
            callbacks.step(step_tics);
            ++playback_step;
            accumulated_tics -= step_tics;
            updating = false;
            ProcessControls();
            // If we're on the last step, then playback is complete and we move back to idling.
            if (IsReplaying() && playback_step == playback_steps.size()) {
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
    const auto bytes = recording.Write();
    // Attempt to save the replay file. If it is too large, or if there is an issue
    // saving it, throw an error
    WriteFile(recording_path, bytes);
}

void ReplaySession::Load(const std::filesystem::path &path) {
    // Try to load the replay file, throw an error if there's an issue doing so
    Load(ReplayRecording::Read(ReadFile(path)));
}

void ReplaySession::Load(ReplayRecording loaded) {
    // Playback needs the same step duration used during recording.
    if (loaded.step_tics != step_tics) {
        throw std::invalid_argument(
            "Replay simulation step duration does not match.");
    }

    std::vector<PlaybackStep> loaded_steps;
    loaded_steps.reserve(loaded.steps.size());
    for (const auto &step : loaded.steps) {
        auto &decoded = loaded_steps.emplace_back();
        decoded.reserve(step.size());
        for (const auto &entry : step) {
            const auto type_id = HashText(entry.type);
            const auto codec = codecs.find(type_id);
            // First we have to check that this event type is registered. If this throws an error,
            // that means that either the replay file is from a different game/version, or that it
            // is being loaded in some different context, where the event was not correctly registered.
            if (codec == codecs.end()) {
                throw std::invalid_argument("Unregistered replay event: " +
                                            entry.type);
            }
            MessageReader payload(entry.payload);
            // If we get here, that means that this event type was registered and we can now decode it.
            auto event = codec->second.decode(payload);
            // If this throws it means someone somewhere goofed. The event was either not encoded properly
            // or not decoded properly.
            if (!event || event->GetTypeId() != type_id ||
                payload.Remaining() != 0) {
                throw std::invalid_argument("Invalid replay event payload: " +
                                            entry.type);
            }
            // If we've gotten through all of that, it means we can finish piecing this event back together
            // and store it as a part of this playback step.
            event->priority = entry.priority;
            event->Handled = false;
            decoded.push_back(std::move(event));
        }
    }

    // Once we've read through the whole replay file, we can store the
    // reconstructed data for later playback.
    recording.initial_state = std::move(loaded.initial_state);
    recording.step_tics = loaded.step_tics;
    recording.steps.clear();
    playback_steps = std::move(loaded_steps);
}

void ReplaySession::StartPlayback() {
    manager.ClearQueuedEvents();
    callbacks.restore(recording.initial_state.bytes);
    playback_step = 0;
    accumulated_tics = 0;
    state = playback_steps.empty() ? State::Idle : State::Playing;
}

} // namespace svanes
