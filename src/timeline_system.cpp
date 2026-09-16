#include <svanes/timeline_system.hpp>

#include <svanes/registry.hpp>

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace svanes {

/**
 * Safely adds two TicCount values, throwing an overflow_error if the result
 * exceeds the maximum representable value for TicCount.
 *
 * @param a The first TicCount value to add.
 * @param b The second TicCount value to add.
 *
 * @return The sum of a and b if it does not overflow.
 *
 * @throws std::overflow_error if the sum of a and b exceeds the maximum
 * representable value for TicCount.
 */
static TicCount CheckedAdd(TicCount a, TicCount b) {
    if (a > std::numeric_limits<TicCount>::max() - b) {
        throw std::overflow_error("Timeline addition exceeds TicCount.");
    }
    return a + b;
}

TicCount SecondsToTics(double seconds) {
    const double tics =
        std::round(seconds * static_cast<double>(TicsPerSecond));

    // If the seconds is negative, nonfinite, or the tics is nonfinite or
    // unrepresentable, throw an exception.
    if (!std::isfinite(seconds) || seconds < 0.0 || !std::isfinite(tics) ||
        tics >= std::ldexp(1.0, 64)) {
        throw std::invalid_argument(
            "SecondsToTics requires a finite, nonnegative duration "
            "representable as tics.");
    }

    return static_cast<TicCount>(tics);
}

float PerSecondToPerTic(float value) {
    return value / static_cast<float>(TicsPerSecond);
}

float PerSecondSquaredToPerTicSquared(float value) {
    return PerSecondToPerTic(PerSecondToPerTic(value));
}

Timeline::Timeline(std::optional<Entity> parent, RationalNumber tic_size)
    : parent(parent) {
    SetTicSize(tic_size);
}

void Timeline::SetParent(std::optional<Entity> new_parent) {
    if (parent == new_parent) {
        return;
    }

    parent = new_parent;
    incomplete_progress = RationalNumber{0};
}

void Timeline::SetTicSize(RationalNumber new_tic_size) {
    if (new_tic_size.GetNumerator() == 0) {
        throw std::invalid_argument(
            "Timeline tic size must be positive; use Pause() to stop time.");
    }

    // Incomplete progress is already a fraction of a local tic. Changing how
    // many parent tics complete the next local tic does not change that
    // progress. We need not change incomplete_progress when changing tic size.

    tic_size = new_tic_size;
}

RationalNumber Timeline::GetTicSize() const { return tic_size; }

void Timeline::Pause() { paused = true; }
void Timeline::Unpause() { paused = false; }

// even more pojo slop

bool Timeline::IsPaused() const { return paused; }

std::optional<Entity> Timeline::GetParent() const { return parent; }
TicCount Timeline::GetTotalTics() const { return total_tics; }
TicCount Timeline::GetDeltaTics() const { return delta_tics; }

void Timeline::Advance(TicCount parent_delta_tics) {
    if (paused) {
        delta_tics = 0;
        return;
    }

    // its like trickle down economics but with time and it actually works

    const RationalNumber progress =
        RationalNumber{parent_delta_tics} / tic_size + incomplete_progress;

    // could throw away these temporaries but then any intermediate exceptions
    // would leave the timeline in a half-advanced state

    const TicCount next_delta = progress.GetWholePart();
    const TicCount next_total = CheckedAdd(total_tics, next_delta);
    incomplete_progress = progress.GetFractionalPart();
    delta_tics = next_delta;
    total_tics = next_total;
}

/**
 * Represents the visitation state of a Timeline during the depth-first
 * traversal of the Timeline hierarchy. This is used to detect cycles in the
 * parent-child relationships of Timelines.
 *
 * A timeline can have one of two...
 * (imagine this popping up like the Invincible logo)
 * MEMBERS:
 * - Visiting: The Timeline is currently being visited in the depth-first
 * traversal.
 * - Complete: The Timeline has been fully visited and processed in the
 * traversal.
 */
enum class TimelineVisit : std::uint8_t { Visiting, Complete };

/**
 * Recursively advances the Timeline of the given entity, ensuring that parent
 * Timelines are advanced before their children. This function performs a
 * depth-first traversal of the Timeline hierarchy, using the visits map to
 * track visitation state and detect cycles.
 *
 * @param world The registry containing all entities and their components.
 * @param entity The entity whose Timeline is to be advanced.
 * @param source_delta_tics The number of tics that have elapsed in the global
 * time source since the last advance. This value is used to advance root
 * Timelines that have no parent.
 * @param visits A map tracking the visitation state of each Timeline during
 * the traversal. Used to detect cycles in the hierarchy.
 *
 * @throws std::logic_error if a cycle is detected in the Timeline hierarchy or
 * if a required parent Timeline is missing. In such cases, the state of the
 * Timelines may be inconsistent.
 */
static void AdvanceTimeline(Registry &world, Entity entity,
                            TicCount source_delta_tics,
                            std::unordered_map<Entity, TimelineVisit> &visits) {

    // We go from child to parent until we reach a root Timeline.
    // If this entity is Visiting, we started
    // exploring it but are still resolving its parent chain. Encountering it
    // again means that chain has looped back to it, so we have a cycle.
    //
    // If it is Complete, it (and its ancestors) have already advanced this
    // pass. We can reach a completed entity through another child sharing that
    // parent, or through the outer ForEach after visiting it as another
    // entity's parent. Skip it so its time advances only once.

    const auto found = visits.find(entity);
    if (found != visits.end()) {
        if (found->second == TimelineVisit::Visiting) {
            throw std::logic_error("Timeline parent cycle at entity " +
                                   std::to_string(entity));
        }
        return;
    }

    visits.emplace(entity, TimelineVisit::Visiting);
    Timeline &timeline = world.GetComponent<Timeline>(entity);
    TicCount delta = source_delta_tics;

    if (const auto parent = timeline.GetParent()) {
        if (!world.HasComponent<Timeline>(*parent)) {
            throw std::logic_error(
                "Timeline for entity " + std::to_string(entity) +
                " requires missing parent Timeline " + std::to_string(*parent));
        }

        // Advance the parent Timeline first, so that this Timeline receives the
        // correct delta from its parent. This ensures that the Timeline
        // hierarchy is advanced in a top-down manner.
        AdvanceTimeline(world, *parent, source_delta_tics, visits);
        delta = world.GetComponent<Timeline>(*parent).GetDeltaTics();
    }

    // At this point, our parent has been advanced (in the case of a root
    // Timeline, our implicit parent is the global time source, so it too has
    // been advanced). We can now advance this Timeline using the delta from its
    // parent.
    timeline.Advance(delta);

    // Mark this Timeline as Complete, indicating that it has been fully
    // processed and its state is now consistent. This prevents re-visiting it
    // in the current traversal and ensures that each Timeline is advanced
    // exactly once per pass.
    visits.at(entity) = TimelineVisit::Complete;
}

void AdvanceTimelines(Registry &world, TicCount source_delta_tics) {

    // cycle detector map
    std::unordered_map<Entity, TimelineVisit> visits;
    world.ForEach<Timeline>([&](Entity entity, Timeline &) {
        AdvanceTimeline(world, entity, source_delta_tics, visits);
    });
}

} // namespace svanes
