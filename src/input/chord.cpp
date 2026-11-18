#include <svanes/input/chord.hpp>

#include <algorithm>
#include <utility>

namespace svanes {

Chord::Chord(std::initializer_list<Key> held) {
    for (Key key : held) {
        keys.set(static_cast<std::size_t>(key));
    }
}

Chord::Chord(Kind kind, Chord first, Chord second, TicCount maxTics,
             InterveningPresses intervening)
    : kind(kind), maxTics(maxTics), intervening(intervening) {
    steps.reserve(2);
    steps.push_back(std::move(first));
    steps.push_back(std::move(second));
}

Chord Chord::Press(Key key) { return Chord({key}); }

Chord Chord::Then(Chord first, Chord second, TicCount maxTics,
                  InterveningPresses intervening) {
    return Chord(Kind::Then, std::move(first), std::move(second), maxTics,
                 intervening);
}

Chord Chord::Within(Chord first, Chord second, TicCount maxTics,
                    InterveningPresses intervening) {

    // If the two steps are the same, we need two separate matches. Using
    // Then makes the second step start after the first one finishes.
    const auto kind = first == second ? Kind::Then : Kind::Within;
    return Chord(kind, std::move(first), std::move(second), maxTics,
                 intervening);
}

bool Chord::KeysPressed(const Sample &sample) const {
    // All the required keys must be down now, and at least one of them must
    // have been up at the start of the poll.
    return (sample.down & keys) == keys && (sample.before & keys) != keys;
}

bool Chord::TriggeredBy(std::span<const Sample> samples) const {
    if (kind == Kind::AllDown) {
        return KeysPressed(samples.back());
    }

    // The last entry can still refer to an earlier match. Check where it
    // ended before reporting that the chord triggered in this poll.
    const auto match = FindMatches(samples).back();
    return match && match->last == samples.size() - 1;
}

Chord::Matches Chord::FindMatches(std::span<const Sample> samples) const {
    // We search the recorded keyboard samples for this chord. Each entry in
    // result gives the start and end of the most recent match through that
    // sample, if any. Then and Within use these entries to look up when
    // each of their two chords matched.
    Matches result;
    result.reserve(samples.size());
    std::optional<Match> latest;
    if (kind == Kind::AllDown) {
        for (std::size_t i = 0; i < samples.size(); ++i) {
            if (KeysPressed(samples[i])) {
                latest = Match{i, i};
            }
            result.push_back(latest);
        }
        return result;
    }

    // Each step is itself a chord. Find the matches for both steps over the
    // same input history so their poll indices line up.
    const auto left = steps[0].FindMatches(samples);
    const auto right = steps[1].FindMatches(samples);

    const auto join = [&](const Matches &first, const Matches &second,
                          std::size_t i) -> std::optional<Match> {
        if (!second[i] || second[i]->last != i) {
            return std::nullopt;
        }

        // For Then, look up the first step in the poll just before the
        // second step started. For Within, the steps may overlap, so the
        // first step can finish as late as the current poll.
        const auto start = second[i]->first;
        if (kind == Kind::Then && start == 0) {
            return std::nullopt;
        }

        const auto &earlier = first[kind == Kind::Then ? start - 1 : i];

        // Measure the time between the polls where the steps finished.
        // A zero-tic window also requires the same poll, since separate
        // polls can have the same timestamp.
        if (!earlier ||
            samples[i].tics - samples[earlier->last].tics > maxTics ||
            (maxTics == 0 && earlier->last != i)) {
            return std::nullopt;
        }

        if (intervening == InterveningPresses::Reject) {
            // Only a new keypress between the steps interrupts the chord.
            // The mask picks out keys that were up before a poll and down
            // at the end of it.
            for (auto j = earlier->last + 1; j < start; ++j) {
                if ((samples[j].down & ~samples[j].before).any()) {
                    return std::nullopt;
                }
            }
        }
        return Match{std::min(earlier->first, start), i};
    };

    for (std::size_t i = 0; i < samples.size(); ++i) {
        auto match = join(left, right, i);
        // Either step can finish last for Within, so try the reverse order
        // if the steps didn't match in the order they were given.
        if (kind == Kind::Within && !match) {
            match = join(right, left, i);
        }

        if (match) {
            latest = match;
        }

        result.push_back(latest);
    }
    return result;
}

} // namespace svanes
