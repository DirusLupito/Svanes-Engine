#pragma once

#include <svanes/timeline_system.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
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

} // namespace double_time
