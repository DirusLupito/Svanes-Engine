#pragma once

#include <svanes/events/event.hpp>

#include <filesystem>
#include <string>

namespace svanes {

/**
 * Specific event type telling the engine to begin logging registered
 * events in order to create a playback log. Games should raise this
 * type of event according to their specific conditions related to their
 * replay systems.
 */
class StartRecordingEvent final : public Event {
public:
    /**
     * Constructor for StartRecordingEvent. This event needs a path
     * so that the engine knows where it should eventually write the completed
     * replay file to.
     *
     * @param path The path to which the engine will save the replay file.
     */
    explicit StartRecordingEvent(std::filesystem::path path);

    /**
     * Returns event type, callable before event object instantiation.
     *
     * @return The event type identifier for starting a recording.
     */
    static std::string GetStaticEventType();

    /**
     * Identifies this event as a request to start recording.
     *
     * @return The event type identifier for starting a recording.
     */
    const std::string GetEventType() const override;

    /**
     * The path that the engine is to write the completed replay file to.
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
     * Returns event type, callable before event object instantiation.
     *
     * @return The event type identifier for stopping a recording.
     */
    static std::string GetStaticEventType();

    /**
     * Identifies this event as a request to stop recording.
     *
     * @return The event type identifier for stopping a recording.
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
     * Constructor for PlayReplayEvent. This event needs a path
     * so that the engine knows where it should look for the replay
     * file to play.
     *
     * @param path The path to the replay file to be played.
     */
    explicit PlayReplayEvent(std::filesystem::path path);

    /**
     * Returns event type, callable before event object instantiation.
     *
     * @return The event type identifier for starting playback.
     */
    static std::string GetStaticEventType();

    /**
     * Identifies this event as a request to start playback.
     *
     * @return The event type identifier for starting playback.
     */
    const std::string GetEventType() const override;

    /**
     * The path to the replay file that is to be played.
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
     * Returns event type, callable before event object instantiation.
     *
     * @return The event type identifier for pausing playback.
     */
    static std::string GetStaticEventType();

    /**
     * Identifies this event as a request to pause playback.
     *
     * @return The event type identifier for pausing playback.
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
     * Returns event type, callable before event object instantiation.
     *
     * @return The event type identifier for resuming playback.
     */
    static std::string GetStaticEventType();

    /**
     * Identifies this event as a request to resume playback.
     *
     * @return The event type identifier for resuming playback.
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
     * Returns event type, callable before event object instantiation.
     *
     * @return The event type identifier for stopping playback.
     */
    static std::string GetStaticEventType();

    /**
     * Identifies this event as a request to stop playback.
     *
     * @return The event type identifier for stopping playback.
     */
    const std::string GetEventType() const override;
};

} // namespace svanes
