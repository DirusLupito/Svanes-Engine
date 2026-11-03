#include <svanes/events/replay_events.hpp>

#include <utility>

namespace svanes {

StartRecordingEvent::StartRecordingEvent(std::filesystem::path path)
    : path(std::move(path)) {}

std::string StartRecordingEvent::GetStaticEventType() {
    return "svanes.replay.record.start";
}

const std::string StartRecordingEvent::GetEventType() const {
    return GetStaticEventType();
}

std::string StopRecordingEvent::GetStaticEventType() {
    return "svanes.replay.record.stop";
}

const std::string StopRecordingEvent::GetEventType() const {
    return GetStaticEventType();
}

PlayReplayEvent::PlayReplayEvent(std::filesystem::path path)
    : path(std::move(path)) {}

std::string PlayReplayEvent::GetStaticEventType() {
    return "svanes.replay.play";
}

const std::string PlayReplayEvent::GetEventType() const {
    return GetStaticEventType();
}

std::string PauseReplayEvent::GetStaticEventType() {
    return "svanes.replay.pause";
}

const std::string PauseReplayEvent::GetEventType() const {
    return GetStaticEventType();
}

std::string ResumeReplayEvent::GetStaticEventType() {
    return "svanes.replay.resume";
}

const std::string ResumeReplayEvent::GetEventType() const {
    return GetStaticEventType();
}

std::string StopReplayEvent::GetStaticEventType() {
    return "svanes.replay.stop";
}

const std::string StopReplayEvent::GetEventType() const {
    return GetStaticEventType();
}

} // namespace svanes
