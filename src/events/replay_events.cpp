#include <svanes/events/replay_events.hpp>

#include <utility>

namespace svanes {

StartRecordingEvent::StartRecordingEvent(std::filesystem::path path)
    : path(std::move(path)) {}

PlayReplayEvent::PlayReplayEvent(std::filesystem::path path)
    : path(std::move(path)) {}

} // namespace svanes
