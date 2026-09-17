#pragma once

#include <svanes/entity.hpp>
#include <svanes/utility/rational_number.hpp>

#include <cstdint>
#include <optional>

namespace svanes {

class Registry;

using TicCount = std::uint64_t;

// A source tic is one tic from the shared simulation clock, counted in
// microseconds. Local tics have this duration only when their timeline and all
// ancestors run at normal speed.
inline constexpr TicCount TicsPerSecond = 1000000;

/**
 * Converts a duration in IRL seconds to whole tics, rounding
 * to nearest (half a tic rounds up). Rejects negative, nonfinite, and
 * unrepresentable durations. Independent of any entity's tic size.
 *
 * @param seconds The duration in seconds to convert to tics.
 *
 * @return The equivalent duration in whole tics.
 *
 * @throws std::invalid_argument if the duration is negative, nonfinite, or
 * unrepresentable as tics.
 *
 */
TicCount SecondsToTics(double seconds);

/**
 * Converts a rate per second to a rate per local tic at normal speed.
 * Independent of any entity's tic size.
 *
 * @param value The rate per second to convert to a rate per local tic.
 *
 * @return The equivalent rate per local tic.
 */
float PerSecondToPerTic(float value);

/**
 * Converts acceleration per second squared to acceleration per local tic
 * squared. Independent of any entity's tic size.
 *
 * @param value The acceleration per second squared to convert to acceleration
 * per local tic squared.
 *
 * @return The equivalent acceleration per local tic squared.
 */
float PerSecondSquaredToPerTicSquared(float value);

/**
 * Represents an entity's local clock. Tic size is parent tics per local tic:
 * 2/1 is half the parent's speed, 1/2 is twice its speed, and 1/1 follows it
 * unchanged.
 * With no parent, the source is the shared simulation clock, measured in
 * microseconds. Normal rate timelines follow completed simulation steps,
 * which can advance more slowly than real time when rendering cannot keep up.
 * A parent identifies another entity's Timeline, not its
 * transform or ownership relationship.
 */
class Timeline {
public:
    /**
     * Constructs a Timeline with an optional parent and tic size.
     *
     * @param parent The optional parent entity whose Timeline this Timeline
     * will follow.
     * @param tic_size The tic size of this Timeline, represented as a
     * RationalNumber, that defines the ratio of parent tics to local tics.
     * Default is 1/1, as in 1 parent tic per local tic.
     *
     * @throws std::invalid_argument if the tic size is zero.
     */
    Timeline(std::optional<Entity> parent = std::nullopt,
             RationalNumber tic_size = RationalNumber{1});

    /**
     * Sets the parent of this Timeline to a new entity.
     * If the new parent is the same as the current parent, no changes are made.
     * Preserves total time but clears incomplete progress when parent changes.
     *
     * @param new_parent The new parent entity whose Timeline this Timeline will
     * follow.
     */
    void SetParent(std::optional<Entity> new_parent);

    /**
     * Sets the tic size of this Timeline to a new value. The tic size is
     * represented as a RationalNumber. If the new tic size is the same as the
     * current tic size, no changes are made. Preserves total and exact
     * incomplete progress. Zero is invalid.
     *
     * @param new_tic_size The new tic size to set for this Timeline.
     *
     * @throws std::invalid_argument if the new tic size is zero.
     */
    void SetTicSize(RationalNumber new_tic_size);

    /**
     * Gets the current tic size of this Timeline.
     *
     * @return The current tic size of this Timeline as a RationalNumber.
     */
    RationalNumber GetTicSize() const;

    /**
     * Pauses the Timeline, stopping the accumulation of time. When paused,
     * Advance() reports zero delta, leaves total time and incomplete progress
     * unchanged, and discards incoming parent time. Changes apply on the next
     * advance, without rewriting the currently reported delta.
     */
    void Pause();

    /**
     * Unpauses the Timeline, allowing it to accumulate time again. When
     * unpaused, the Timeline will resume advancing based on its parent or, for
     * a root Timeline, the shared source clock. Changes apply on the next
     * advance, without rewriting the currently reported delta.
     */
    void Unpause();

    /**
     * Checks if the Timeline is currently paused.
     *
     * @return true if the Timeline is paused, false otherwise.
     */
    bool IsPaused() const;

    /**
     * Gets the current parent entity of this Timeline, if any.
     *
     * @return The optional parent entity of this Timeline.
     */
    std::optional<Entity> GetParent() const;

    /**
     * Gets the total number of local tics that have elapsed since the Timeline
     * was created.
     *
     * @return The total number of local tics.
     */
    TicCount GetTotalTics() const;

    /**
     * Gets the number of whole local tics reported by the most recent
     * Advance(). Reading this value does not consume it.
     *
     * @return The whole local tic delta reported by the most recent advance.
     */
    TicCount GetDeltaTics() const;

    /**
     * Advances the Timeline by the given number of parent tics, updating the
     * total and delta tics accordingly. If the Timeline is paused, the delta
     * tics will be set to zero and the total tics will remain unchanged. The
     * engine shall call this method once per update pass for each Timeline,
     * with parents being advanced before their children. Can be used to
     * manually advance a Timeline outside of the engine's update loop, but care
     * must be taken to ensure that parent Timelines are advanced before their
     * children to maintain correct time progression.
     *
     * @param parent_delta_tics The number of tics that have elapsed in the
     * parent Timeline since the last advance. If the Timeline has no parent,
     * this should be the number of tics that have elapsed in the global time
     * source.
     *
     * @throws std::overflow_error if fraction arithmetic or accumulated time
     * cannot be represented. This Timeline remains unchanged on failure.
     */
    void Advance(TicCount parent_delta_tics);

private:
    // The parent entity whose Timeline this Timeline follows, if any.
    std::optional<Entity> parent;

    // The tic size of this Timeline, represented as a RationalNumber, defining
    // the ratio of parent tics to local tics. Default is 1/1, meaning 1 parent
    // tic per local tic.
    RationalNumber tic_size{1};

    // Indicates whether the Timeline is currently paused. When paused, the
    // Timeline will not advance.
    bool paused = false;

    // The total number of local tics that have elapsed since the Timeline was
    // created.
    TicCount total_tics = 0;

    // The number of whole local tics reported by the most recent Advance().
    TicCount delta_tics = 0;

    // The incomplete fraction of one LOCAL tic, kept exactly as a rational.
    // For example, tic size 2/3 means two parent tics per three local tics.
    // Advancing by five parent tics gives 5 / (2/3) = 15/2 local tics in total:
    // seven whole tics are reported, and incomplete_progress becomes 1/2.
    // That 1/2 is half of one local tic, not half of a parent tic.
    RationalNumber incomplete_progress{0};
};

// We may want to consider updating the registry to have a special submap
// for just entities with timelines, so we can iterate over them without
// checking every entity.

/**
 * Advances every Timeline once, parents before children. Roots receive
 * source_delta_tics from the shared simulation clock. A source tic is one
 * clock tic, counted in microseconds here. Children receive their parent's
 * reported local delta. The application supplies a fixed step of simulated
 * microseconds, but callers can supply a manual source. Throws for missing
 * parent timelines, cycles, or arithmetic overflow. Run this before anything
 * needs to check anything time related.
 *
 * In the engine's main loop, this runs once per simulation step, not once per
 * rendered frame. The loop accumulates real time and calls this at most once
 * per frame with the fixed step size when a complete interval is due. If
 * 35000 real tics have accumulated and the step is 10000, it calls this once
 * with 10000, carries 5000 into the next frame, and discards 20000 overdue
 * tics. Only completed steps advance timelines. Discarded steps do not.
 * This slows simulated time under overload without changing integration
 * granularity. Standalone timelines can still be advanced from another source.
 *
 * @param world The registry containing all entities whose Timeline components
 * will be advanced.
 * @param source_delta_tics The number of source tics that have elapsed in the
 * shared time source since the last advance. A source tic is one tic from that
 * clock, counted in microseconds here. This value is used to advance root
 * Timelines that have no parent, and is therefore the basis for all other
 * Timelines in the hierarchy. It should be a non-negative value representing
 * the elapsed time in whole tics.
 *
 * @throws std::logic_error if any parent Timeline is missing or if there are
 * cycles in the Timeline hierarchy.
 * @throws std::overflow_error if arithmetic overflows during the advancement
 * of Timelines. In such cases, the state of the Timelines may
 * be partially advanced, and it is not rolled back.
 *
 */
void AdvanceTimelines(Registry &world, TicCount source_delta_tics);

} // namespace svanes
