#pragma once

#include <svanes/input/input_types.hpp>
#include <svanes/timeline_system.hpp>

#include <bitset>
#include <cstddef>
#include <initializer_list>
#include <optional>
#include <span>
#include <vector>

namespace svanes {

/**
 * A keyboard binding a game can define and query at any time.
 */
class Chord final {
public:
    /**
     * Controls whether another keypress can occur between two chord steps.
     *
     * MEMBERS:
     * - Allow: Other keys may be pressed between the two steps.
     * - Reject: A keypress between the two steps interrupts the sequence.
     */
    enum class InterveningPresses { Allow, Reject };

    /**
     * Matches the poll in which a key goes down.
     *
     * @param key The key to watch.
     *
     * @return A chord for that keypress.
     *
     * @throws std::out_of_range if key is invalid.
     */
    static Chord Press(Key key);

    /**
     * Matches when the last of a group of keys goes down.
     *
     * @tparam Keys The Key types deduced from the remaining arguments.
     *
     * @param first The first required key.
     * @param rest The other keys that must be held with it, in any order.
     *
     * @return A chord that triggers when the keys are first held together.
     *
     * @throws std::out_of_range if any key is invalid.
     */
    template <typename... Keys> static Chord AllDown(Key first, Keys... rest) {
        return Chord({first, rest...});
    }

    /**
     * Matches two chords in order. The second must start in a later poll.
     *
     * @param first The chord that must complete first.
     * @param second The chord that follows it.
     * @param maxTics The maximum time between completions, in microseconds.
     * @param intervening Whether other keypresses may occur between steps.
     *
     * @return The ordered sequence.
     */
    static Chord
    Then(Chord first, Chord second, TicCount maxTics,
         InterveningPresses intervening = InterveningPresses::Allow);

    /**
     * Matches two chords in either order. Repeating a chord requires two
     * separate occurrences.
     *
     * @param first One of the two chords.
     * @param second The other chord.
     * @param maxTics The maximum time between completions, in microseconds.
     * Zero requires both to complete in the same poll.
     * @param intervening Whether other keypresses may occur between steps.
     *
     * @return A chord that triggers when both steps have occurred.
     */
    static Chord
    Within(Chord first, Chord second, TicCount maxTics,
           InterveningPresses intervening = InterveningPresses::Allow);

    /**
     * Compares two chord definitions.
     *
     * @param other The chord to compare with this one.
     *
     * @return True if both define the same keys, steps, and matching rules.
     */
    bool operator==(const Chord &other) const = default;

private:
    // The input manager records the samples used to check a chord.
    friend class InputManager;

    // A set of keys indexed by their Key values.
    using KeyState = std::bitset<static_cast<std::size_t>(Key::Count)>;

    /**
     * A keyboard sample from one input poll.
     *
     * FIELDS:
     * - tics: The time of the poll in unscaled microseconds.
     * - before: The keys held just before the poll.
     * - down: The keys held at the end of the poll.
     */
    struct Sample {
        TicCount tics;
        KeyState before;
        KeyState down;
    };

    /**
     * Identifies how to match this chord.
     *
     * MEMBERS:
     * - AllDown: The required keys are held together.
     * - Then: The two steps occur in the given order.
     * - Within: The two steps may occur in either order.
     */
    enum class Kind { AllDown, Then, Within };

    /**
     * Constructs a chord from keys that must be held together.
     *
     * @param held The keys required to trigger the chord.
     * @throws std::out_of_range if any key is invalid.
     */
    explicit Chord(std::initializer_list<Key> held);

    /**
     * Combines two chords into a timed binding.
     *
     * @param kind The ordering rule for the two steps.
     * @param first The first step in the definition.
     * @param second The second step in the definition.
     * @param maxTics The maximum time between completions, in microseconds.
     * @param intervening Whether other keys may be pressed between steps.
     */
    Chord(Kind kind, Chord first, Chord second, TicCount maxTics,
          InterveningPresses intervening);

    /**
     * The span of polls in which one occurrence of a chord took place.
     *
     * FIELDS:
     * - first: The index of the sample where the match began.
     * - last: The index of the sample where the chord completed.
     */
    struct Match {
        std::size_t first;
        std::size_t last;
    };
    // The most recent match as of each sample, in poll order.
    using Matches = std::vector<std::optional<Match>>;

    /**
     * Checks whether the required keys became held together in a poll.
     *
     * @param sample The keyboard sample to check.
     *
     * @return True if the poll supplied the last required keypress.
     */
    bool KeysPressed(const Sample &sample) const;

    /**
     * Checks whether the latest poll completed this chord.
     *
     * @param samples The polls in chronological order, ending with the
     * current poll. Must contain at least one sample.
     *
     * @return True if the chord completed in the final sample.
     */
    bool TriggeredBy(std::span<const Sample> samples) const;

    /**
     * Finds the most recent match at each point in the input history.
     *
     * @param samples The keyboard samples in chronological order.
     *
     * @return One entry per sample. An entry is empty until the first match
     * and retains that match until another one completes.
     */
    Matches FindMatches(std::span<const Sample> samples) const;

    // The matching rule for this chord.
    Kind kind = Kind::AllDown;

    // The keys required by an AllDown chord.
    KeyState keys;

    // The two chords combined by Then or Within, in the order supplied.
    std::vector<Chord> steps;

    // The maximum time between the two steps' completions, in microseconds.
    TicCount maxTics = 0;

    // Whether other keypresses are allowed between the two steps.
    InterveningPresses intervening = InterveningPresses::Allow;
};

} // namespace svanes
