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
 * Specific event type telling the engine to begin logging registered
 * events in order to create a playback log. Games should raise this
 * type of event according to their specific conditions related to their
 * replay systems.
*/
class StartRecordingEvent final : public Event {
public:
    /** 
     * Constructor for StartRecordingEvent. This event needs a path to 
     * so that the engine knows where it should eventually write the completed
     * replay file to.
     * 
     * @param path The path to which the engine will save the replay file.
    */
    explicit StartRecordingEvent(std::filesystem::path path);
    /** 
     * Returns event type, callable before event object instantiation
    */
    static std::string GetStaticEventType();
    /**
     * Implementation of GetEventType required by Event
     */
    const std::string GetEventType() const override;
    /** 
     * The path that the engine is to write the completed replay file to
    */
    std::filesystem::path path;
};

/** 
 * Specific event type telling the engine to stop logging registered
 * events and finish creating the playback log. Games should raise this
 * type of event according to their specific conditions related to their
 * replay systems.
*/
class StopRecordingEvent final : public Event {
public:
    /** 
     * Returns event type, callable before event object instantiation
    */
    static std::string GetStaticEventType();
    /**
     * Implementation of GetEventType required by Event
     */
    const std::string GetEventType() const override;
};

/** 
 * Specific event type telling the engine to begin playback for a
 * replay session. Games should raise this type of event according 
 * to their specific conditions related to their replay systems.
*/
class PlayReplayEvent final : public Event {
public:
    /** 
     * Constructor for PlayReplayEvent. This event needs a path to 
     * so that the engine knows where it should look for the replay
     * file to play.
     * 
     * @param path The path to the replay file to be played.
    */
    explicit PlayReplayEvent(std::filesystem::path path);
    /** 
     * Returns event type, callable before event object instantiation
    */
    static std::string GetStaticEventType();
    /**
     * Implementation of GetEventType required by Event
     */
    const std::string GetEventType() const override;
    /** 
     * The path to the replay file that is to be played
    */
    std::filesystem::path path;
};

/** 
 * Specific event type telling the engine to pause playback for a
 * replay session. Games should raise this type of event according 
 * to their specific conditions related to their replay systems.
*/
class PauseReplayEvent final : public Event {
public:
    /** 
     * Returns event type, callable before event object instantiation
    */
    static std::string GetStaticEventType();
    /**
     * Implementation of GetEventType required by Event
     */
    const std::string GetEventType() const override;
};

/** 
 * Specific event type telling the engine to resume playback for a
 * replay session. Games should raise this type of event according 
 * to their specific conditions related to their replay systems.
*/
class ResumeReplayEvent final : public Event {
public:
    /** 
     * Returns event type, callable before event object instantiation
    */
    static std::string GetStaticEventType();
    /**
     * Implementation of GetEventType required by Event
     */
    const std::string GetEventType() const override;
};

/** 
 * Specific event type telling the engine to stop playback for a
 * replay session. Games should raise this type of event according 
 * to their specific conditions related to their replay systems.
*/
class StopReplayEvent final : public Event {
public:
    /** 
     * Returns event type, callable before event object instantiation
    */
    static std::string GetStaticEventType();
    /**
     * Implementation of GetEventType required by Event
     */
    const std::string GetEventType() const override;
};

/**
 * Representation of a replay playback session. Requires some game-side
 * setup for collback functions and event registration. Provides basic
 * functionality for navigating replay playback.
 */
class ReplaySession final {
public:
    /** 
     * Representation of the current status of a ReplaySession
    */
    enum class State { Idle, Recording, Playing, Paused };

    /** 
     * Game provided callback functions necessary for correctly executing
     * playback.
    */
    struct Callbacks {
        /** 
         * Captures the initial game state on recording start. This state is
         * stored in the replay file.
        */
        std::function<NetworkMessage()> save;
        /** 
         * Takes the state captured by save and recreates it when playback begins.
        */
        std::function<void(std::span<const std::byte>)> restore;
        /** 
         * Called once every simulation step, advances the playback according to 
         * game-defined simulation logic.
        */
        std::function<void(TicCount)> step;
    };

    /** 
     * Constructor for a replay session. A replay session is to be associated with
     * a specific EventManager, and requires game-defined callbacks.
     * 
     * @param manager The event manager associated with this ReplaySession
     * @param step_tics The simulation step duration used during recording and playback
     * @param callbacks The callback functions used for handling initial state and simulation logic
    */
    ReplaySession(EventManager &manager, TicCount step_tics,
                  Callbacks callbacks);
    /** 
     * Default destructor for ReplaySessions
    */
    ~ReplaySession();
    /** 
     * Check to make sure a replay session is not a copy of another
    */
    ReplaySession(const ReplaySession &) = delete;
    /** 
     * Check to prevent overriding an existing session
    */
    ReplaySession &operator=(const ReplaySession &) = delete;
    /** 
     * Check to prevent moving a ReplaySession into a new object
    */
    ReplaySession(ReplaySession &&) = delete;
    /** 
     * Check to prevent moving a ReplaySession into an existing object
    */
    ReplaySession &operator=(ReplaySession &&) = delete;

    /** 
     * Function for games to register their events for use in replays.
     * Event types must be registered in order to be logged, and thus
     * played back. 
     * 
     * @param encode Algorithm to encode an event for writing to the log
     * @param decode Algorithm to decode this type of event from the log for use in playback
    */
    template <std::derived_from<Event> T, typename Encode, typename Decode>
    void RegisterEvent(Encode encode, Decode decode) {
        RegisterCodec(
            T::GetStaticEventType(),
            Codec{[encode = std::move(encode)](MessageWriter &writer,
                                               const Event &event) {
                      encode(writer, dynamic_cast<const T &>(event));
                  },
                  [decode = std::move(decode)](MessageReader &reader)
                      -> std::unique_ptr<Event> { return decode(reader); }});
    }

    /** 
     * Handles registering player inputs for playback. Because just about all
     * games will have inputs that will need to be included for playback construction,
     * we don't need to have each developer register them themselves. Instead they
     * can just call this and have it taken care of for them.
    */
    void RegisterInputEvents();
    /** 
     * Takes a pending action and decides what to do with it based on the current
     * playback state, including handling cases of invalid state update requests.
    */
    void ProcessControls();
    /**
     * Opens the recording window at the beginning of a simulation step.
     */
    void BeginStep();
    /** 
     * Closes the recording window at the end of a simulation step.
    */
    void EndStep();
    /** 
     * Advances playback and calls the game-defined step callback.
     * 
     * @param elapsed_tics The number of tics elapsed since the last call
    */
    void Update(TicCount elapsed_tics);
    /** 
     * Returns the current state of the ReplaySession
    */
    State GetState() const;
    /** 
     * Returns true if the ReplaySession is carrying out playback.
    */
    bool IsReplaying() const;

private:
    friend class EventManager;

    enum class Action { Record, Save, Play, Pause, Resume, Stop };

    struct Control {
        Action action;
        std::filesystem::path path;
    };

    struct Codec {
        std::function<void(MessageWriter &, const Event &)> encode;
        std::function<std::unique_ptr<Event>(MessageReader &)> decode;
    };

    struct RecordedEvent {
        std::string type;
        EventPriority priority;
        NetworkMessage payload;
        std::unique_ptr<Event> decoded;
    };

    using Step = std::vector<RecordedEvent>;

    void RegisterCodec(std::string type, Codec codec);
    bool Observe(const Event &event);
    void Reset();
    void Save() const;
    void Load(const std::filesystem::path &path);
    void CheckBoundary() const;

    EventManager *manager;
    TicCount step_tics;
    Callbacks callbacks;
    State state = State::Idle;
    std::unordered_map<std::string, Codec> codecs;
    std::vector<Control> controls;
    std::filesystem::path recording_path;
    NetworkMessage initial_state;
    std::vector<Step> steps;
    Step pending_events;
    std::size_t playback_step = 0;
    TicCount accumulated_tics = 0;
    bool step_open = false;
    bool updating = false;
    bool injecting = false;
};

}
