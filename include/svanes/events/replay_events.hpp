#pragma once

#include <svanes/events/event.hpp>

#include <filesystem>
#include <string_view>

namespace svanes {

/**
 * Specific event type telling the engine to begin logging registered
 * events in order to create a playback log. Games should raise this
 * type of event according to their specific conditions related to their
 * replay systems.
 */
class StartRecordingEvent final : public EventOf<StartRecordingEvent> {
public:
    /**
     * The unique name identifying requests to start recording.
     */
    static constexpr std::string_view EventName = "svanes.replay.record.start";

    /**
     * Constructor for StartRecordingEvent. This event needs a path
     * so that the engine knows where it should eventually write the completed
     * replay file to.
     *
     * @param path The path to which the engine will save the replay file.
     */
    explicit StartRecordingEvent(std::filesystem::path path);

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
class StopRecordingEvent final : public EventOf<StopRecordingEvent> {
public:
    /**
     * The unique name identifying requests to stop recording.
     */
    static constexpr std::string_view EventName = "svanes.replay.record.stop";
};

/**
 * Specific event type telling the engine to begin playback for a
 * replay session. Games should raise this type of event according
 * to their specific conditions related to their replay systems.
 */
class PlayReplayEvent final : public EventOf<PlayReplayEvent> {
public:
    /**
     * The unique name identifying requests to start replay playback.
     */
    static constexpr std::string_view EventName = "svanes.replay.play";

    /**
     * Constructor for PlayReplayEvent. This event needs a path
     * so that the engine knows where it should look for the replay
     * file to play.
     *
     * @param path The path to the replay file to be played.
     */
    explicit PlayReplayEvent(std::filesystem::path path);

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
class PauseReplayEvent final : public EventOf<PauseReplayEvent> {
public:
    /**
     * The unique name identifying requests to pause replay playback.
     */
    static constexpr std::string_view EventName = "svanes.replay.pause";
};

/**
 * Specific event type telling the engine to resume playback for a
 * replay session. Games should raise this type of event according
 * to their specific conditions related to their replay systems.
 */
class ResumeReplayEvent final : public EventOf<ResumeReplayEvent> {
public:
    /**
     * The unique name identifying requests to resume replay playback.
     */
    static constexpr std::string_view EventName = "svanes.replay.resume";
};

/**
 * Specific event type telling the engine to stop playback for a
 * replay session. Games should raise this type of event according
 * to their specific conditions related to their replay systems.
 */
class StopReplayEvent final : public EventOf<StopReplayEvent> {
public:
    /**
     * The unique name identifying requests to stop replay playback.
     */
    static constexpr std::string_view EventName = "svanes.replay.stop";
};

} // namespace svanes
