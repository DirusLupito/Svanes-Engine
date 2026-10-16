#pragma once

#include <svanes/timeline_system.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>

namespace double_time {

inline double BeatDurationTics(std::uint32_t beats_per_minute) {
    if (beats_per_minute == 0) {
        throw std::invalid_argument("Rhythm BPM must be positive.");
    }
    return static_cast<double>(svanes::TicsPerSecond) * 60.0 /
           static_cast<double>(beats_per_minute);
}

inline std::uint64_t BeatIndex(svanes::TicCount timeline_tics,
                               std::uint32_t beats_per_minute) {
    return static_cast<std::uint64_t>(std::floor(
        static_cast<double>(timeline_tics) /
        BeatDurationTics(beats_per_minute)));
}

inline bool IsOnBeat(svanes::TicCount timeline_tics,
                     std::uint32_t beats_per_minute,
                     double tolerance) {
    if (!std::isfinite(tolerance) || tolerance < 0.0 ||
        tolerance > 0.5) {
        throw std::invalid_argument(
            "Rhythm tolerance must be between zero and one half of a beat.");
    }

    const double beat_tics = BeatDurationTics(beats_per_minute);
    const double phase = std::fmod(static_cast<double>(timeline_tics),
                                   beat_tics);
    const double distance_to_boundary = std::min(phase, beat_tics - phase);
    return distance_to_boundary <= beat_tics * tolerance;
}

class RhythmBeatEmitter {
public:
    std::uint64_t Poll(svanes::TicCount timeline_tics,
                       std::uint32_t beats_per_minute) {
        const auto beat_index = BeatIndex(timeline_tics, beats_per_minute);

        if (!last_beat_index_.has_value()) {
            last_beat_index_ = beat_index;
            return 0;
        }
        if (beat_index < *last_beat_index_) {
            last_beat_index_ = beat_index;
            return 0;
        }

        const std::uint64_t crossed = beat_index - *last_beat_index_;
        last_beat_index_ = beat_index;
        return crossed;
    }

    void Reset() { last_beat_index_.reset(); }

private:
    std::optional<std::uint64_t> last_beat_index_;
};

class RhythmClock {
public:
    RhythmClock(const svanes::Timeline &timeline,
                std::uint32_t beats_per_minute)
        : timeline_(&timeline), beats_per_minute_(beats_per_minute) {
        (void)BeatDurationTics(beats_per_minute_);
    }

    void SetBeatsPerMinute(std::uint32_t beats_per_minute) {
        (void)BeatDurationTics(beats_per_minute);
        beats_per_minute_ = beats_per_minute;
        beat_emitter_.Reset();
    }

    bool IsOnBeat(double tolerance_fraction) const {
        return double_time::IsOnBeat(timeline_->GetTotalTics(),
                                     beats_per_minute_, tolerance_fraction);
    }

    std::uint64_t Poll() {
        return beat_emitter_.Poll(timeline_->GetTotalTics(),
                                  beats_per_minute_);
    }

private:
    const svanes::Timeline *timeline_;
    std::uint32_t beats_per_minute_;
    RhythmBeatEmitter beat_emitter_;
};

} // namespace double_time
