#pragma once

#include <svanes/events/event.hpp>
#include <svanes/network/message_serialization.hpp>
#include <svanes/timeline_system.hpp>

#include <concepts>
#include <filesystem>
#include <functional>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace svanes {

class EventManager;

/**
 * Representation of a replay recording and playback session. Requires some
 * game-side setup for callback functions and event registration. Provides basic
 * functionality for navigating replay playback.
 */
class ReplaySession final {
public:
    /**
     * Representation of the current status of a ReplaySession.
     *
     * MEMBERS:
     * - Idle: No recording or playback is active.
     * - Recording: Collecting registered events and completed simulation steps.
     * - Playing: Advancing through recorded simulation steps.
     * - Paused: Playback is active but does not advance.
     */
    enum class State { Idle, Recording, Playing, Paused };

    /**
     * Game provided callback functions necessary for recording and correctly
     * executing playback.
     *
     * FIELDS:
     * - save: Serializes the game state at the start of recording.
     * - restore: Recreates the game from the saved starting state.
     * - step: Advances the game's simulation by one playback step.
     */
    struct Callbacks {
        /**
         * Captures the initial game state on recording start. This state is
         * stored in the replay file. Returns the serialized state as message
         * bytes.
         */
        std::function<NetworkMessage()> save;

        /**
         * Takes the state captured by save and recreates it when playback
         * begins. Receives the serialized state as a borrowed byte span.
         */
        std::function<void(std::span<const std::byte>)> restore;

        /**
         * Called once every playback simulation step to advance according to
         * game-defined simulation logic. Receives the step duration in unscaled
         * source tics.
         */
        std::function<void(TicCount)> step;
    };

    /**
     * Constructor for a replay session. A replay session is to be associated
     * with a specific EventManager, and requires game-defined callbacks.
     *
     * @param manager The event manager associated with this ReplaySession.
     * @param step_tics The simulation step duration used during recording and
     * playback, in unscaled source tics. Must be positive.
     * @param callbacks The callback functions used for handling initial state
     * and simulation logic. All three functions must be provided.
     *
     * @throws std::invalid_argument if step_tics is zero or a callback is
     * empty.
     */
    ReplaySession(EventManager &manager, TicCount step_tics,
                  Callbacks callbacks);

    /**
     * Override of the copy constructor to prevent copying of ReplaySession
     * instances, which are associated with a specific event manager.
     *
     * This prevents something like:
     * ReplaySession session1(manager, step_tics, callbacks);
     * ReplaySession session2 = session1;
     */
    ReplaySession(const ReplaySession &) = delete;

    /**
     * Override of the copy assignment operator to prevent copying of
     * ReplaySession instances.
     *
     * This prevents something like:
     * ReplaySession session1(manager1, step_tics, callbacks);
     * ReplaySession session2(manager2, step_tics, callbacks);
     * session2 = session1;
     */
    ReplaySession &operator=(const ReplaySession &) = delete;

    /**
     * Override of the move constructor to prevent moving of ReplaySession
     * instances. The event manager refers to the session at its original
     * address.
     *
     * This prevents something like:
     * ReplaySession session1(manager, step_tics, callbacks);
     * ReplaySession session2 = std::move(session1);
     */
    ReplaySession(ReplaySession &&) = delete;

    /**
     * Override of the move assignment operator to prevent moving of
     * ReplaySession instances.
     *
     * This prevents something like:
     * ReplaySession session1(manager1, step_tics, callbacks);
     * ReplaySession session2(manager2, step_tics, callbacks);
     * session2 = std::move(session1);
     */
    ReplaySession &operator=(ReplaySession &&) = delete;

    /**
     * Function for games to register their events for use in replays.
     * Event types must be registered in order to be logged, and thus
     * played back.
     *
     * @tparam T The event type to record and play back. T's EventName must be
     * unique.
     * @tparam Encode The type of the encoding callable.
     * @tparam Decode The type of the decoding callable.
     * @param encode Algorithm to encode an event for writing to the log.
     * Receives a MessageWriter and the event whose payload is to be written.
     * @param decode Algorithm to decode this type of event from the log for use
     * in playback. Receives a MessageReader and returns an owning event
     * pointer. It must consume the complete payload without changing the game
     * world.
     *
     * @throws std::invalid_argument if the event type identifier is empty,
     * reserved for replay controls, or already registered.
     * @throws std::logic_error if the session is not idle, or registration
     * occurs while processing events, a step, or a control action.
     */
    template <std::derived_from<Event> T, typename Encode, typename Decode>
    void RegisterEvent(Encode encode, Decode decode) {
        RegisterCodec(
            T::EventName,
            Codec{[encode = std::move(encode)](MessageWriter &writer,
                                               const Event &event) {
                      encode(writer, static_cast<const T &>(event));
                  },
                  [decode = std::move(decode)](MessageReader &reader)
                      -> std::unique_ptr<Event> { return decode(reader); }});
    }

    /**
     * Handles registering player inputs for playback. Includes key presses and
     * releases, mouse button changes, and mouse movement.
     *
     * @throws std::invalid_argument if an input event type is already
     * registered.
     * @throws std::logic_error if the session is not idle, or registration
     * occurs while processing events, a step, or a control action.
     */
    void RegisterInputEvents();

    /**
     * Takes pending actions and decides what to do with them based on the
     * current playback state, including handling cases of invalid state update
     * requests.
     *
     * @throws std::logic_error if an action requests an invalid state change,
     * or called while processing events, a step, or another control action.
     * @throws ... Propagates file and game callback errors from pending
     * actions.
     */
    void ProcessControls();

    /**
     * Opens the recording window at the beginning of a live simulation step.
     * Processes pending replay controls first.
     *
     * @throws std::logic_error if playback is active, or called while
     * processing events, a step, or a control action.
     * @throws ... Propagates errors from pending replay controls.
     */
    void BeginStep();

    /**
     * Closes the recording window at the end of a simulation step.
     * Stores the completed step when recording, even if it contains no events,
     * then processes pending replay controls.
     *
     * @throws std::logic_error if no live step is open, or events are being
     * delivered.
     * @throws ... Propagates errors from pending replay controls.
     */
    void EndStep();

    /**
     * Advances playback and calls the game-defined step callback.
     * Runs complete recorded steps and retains any fractional step time.
     *
     * @param elapsed_tics The number of unscaled source tics elapsed since
     * the last call.
     *
     * @throws std::logic_error if called while processing events, a step,
     * or another control action.
     * @throws std::overflow_error if elapsed_tics would overflow accumulated
     * time.
     * @throws ... Propagates errors from replay controls, event handlers, or
     * the game-defined step callback.
     */
    void Update(TicCount elapsed_tics);

    /**
     * Returns the current state of the ReplaySession.
     *
     * @return The current session state.
     */
    State GetState() const;

    /**
     * Returns true if the ReplaySession is carrying out playback.
     *
     * @return true while playing or paused, false otherwise.
     */
    bool IsReplaying() const;

private:
    // The event manager needs to be able to access the session's internals to
    // observe events and queue replay actions.
    friend class EventManager;

    /**
     * A replay control action to apply at a simulation boundary.
     *
     * MEMBERS:
     * - Record: Capture the starting state and begin recording.
     * - Save: Write the recording to its file and stop recording.
     * - Play: Load a replay, restore its starting state, and begin playback.
     * - Pause: Pause playback without discarding fractional progress.
     * - Resume: Continue paused playback.
     * - Stop: Stop playback and discard pending events.
     */
    enum class Action { Record, Save, Play, Pause, Resume, Stop };

    /**
     * A replay control request waiting to be applied.
     *
     * FIELDS:
     * - action: The recording or playback action to perform.
     * - path: The recording destination or replay source for Record and Play.
     * Empty for the other actions.
     */
    struct Control {
        Action action;
        std::filesystem::path path;
    };

    /**
     * Encoding and decoding functions for one registered event type.
     *
     * FIELDS:
     * - encode: Writes an event's payload through the supplied MessageWriter.
     * - decode: Reads the payload through the supplied MessageReader and
     * returns an owning event pointer.
     */
    struct Codec {
        std::function<void(MessageWriter &, const Event &)> encode;
        std::function<std::unique_ptr<Event>(MessageReader &)> decode;
    };

    /**
     * An event saved during recording or reconstructed for playback.
     *
     * FIELDS:
     * - type: The event type's name, which the replay file stores and Load
     * hashes to select its codec.
     * - priority: The event's original dispatch priority.
     * - payload: The encoded event payload, populated while recording.
     * - decoded: The reconstructed event used during playback, empty while
     * recording.
     */
    struct RecordedEvent {
        std::string type;
        EventPriority priority;
        NetworkMessage payload;
        std::unique_ptr<Event> decoded;
    };

    // The replayable events from one simulation step, in delivery order.
    using Step = std::vector<RecordedEvent>;

    /**
     * Registers encoding and decoding logic for use in reading and writing
     * replay files. Registration must occur while the session is idle, and
     * duplicates cannot be registered.
     *
     * @param type The name of the event type associated with the codec.
     * @param codec The functions used to encode and decode event payloads.
     *
     * @throws std::invalid_argument if type is empty, reserved for replay
     * controls, or already registered.
     * @throws std::logic_error if the session is not idle, or registration
     * occurs while processing events, a step, or a control action.
     */
    void RegisterCodec(std::string_view type, Codec codec);

    /**
     * Watches for events and decides what to do based on their type. If it's
     * a replay event, queues the relevant replay action for the session to
     * deal with. Otherwise, adds registered events to the log while recording.
     *
     * @param event The event being delivered by the associated manager.
     *
     * @return false if a live event of a registered type is suppressed during
     * playback, true if the event should reach its handlers.
     *
     * @throws std::logic_error if a registered event is delivered while
     * recording without an open simulation step.
     * @throws ... Propagates errors from the event encoder.
     */
    bool Observe(const Event &event);

    /**
     * Completely resets the ReplaySession to an idle state and clears out
     * pending events and tasks. Discards recorded and loaded replay data.
     *
     * @throws std::logic_error if called while processing events, a step,
     * or a control action.
     */
    void Reset();

    /**
     * Writes the recorded starting state and completed simulation steps to
     * the selected replay file.
     *
     * @throws std::length_error if the recording or an event type identifier
     * is too large for the file format.
     * @throws std::runtime_error if the file cannot be written.
     */
    void Save() const;

    /**
     * Reads a replay file and reconstructs its starting state and events
     * for playback.
     *
     * @param path The replay file to read.
     *
     * @throws std::runtime_error if the file cannot be opened or read.
     * @throws std::length_error if the file size is invalid or too large.
     * @throws std::invalid_argument if the format, step duration, event types,
     * or payloads are invalid, or bytes are missing or left over.
     * @throws ... Propagates errors from event decoders.
     */
    void Load(const std::filesystem::path &path);

    /**
     * Checks that replay operations occur at a simulation boundary, so they
     * cannot change the session partway through event delivery or a step.
     *
     * @throws std::logic_error if events are being delivered, a live step
     * is open, or replay controls or playback steps are being processed.
     */
    void CheckBoundary() const;

    // The associated event manager. Not owned by this session.
    EventManager &manager;

    // The duration of one simulation step, in unscaled source tics.
    TicCount step_tics;

    // The game-defined state capture, restoration, and simulation callbacks.
    Callbacks callbacks;

    // The session's current recording or playback state.
    State state = State::Idle;

    // The encoding and decoding functions keyed by event type ID.
    std::unordered_map<EventTypeId, Codec> codecs;

    // The replay control actions waiting for the next simulation boundary.
    std::vector<Control> controls;

    // The destination path selected when recording starts.
    std::filesystem::path recording_path;

    // The serialized starting state captured for recording or loaded from a
    // file.
    NetworkMessage initial_state;

    // The completed recorded steps, or the steps loaded for playback.
    std::vector<Step> steps;

    // The events collected for the live step currently being recorded.
    Step pending_events;

    // The index of the next recorded step to run during playback.
    std::size_t playback_step = 0;

    // The unscaled elapsed time accumulated toward the next playback step.
    TicCount accumulated_tics = 0;

    // Whether a live simulation step is open.
    bool step_open = false;

    // Whether replay controls or a playback step are being processed.
    bool updating = false;

    // Whether the manager is delivering an event taken from the replay.
    bool injecting = false;
};

} // namespace svanes
