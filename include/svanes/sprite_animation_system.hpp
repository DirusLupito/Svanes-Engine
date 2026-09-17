#pragma once

#include <cstdint>

#include <svanes/render/render_system.hpp>
#include <svanes/timeline_system.hpp>

namespace svanes {

class Registry;

/**
 * Represents a sprite animation in a 2D game.
 *
 * frame_width - Width of each frame in the sprite sheet.
 * frame_height - Height of each frame in the sprite sheet.
 * frame_count - Total number of frames in the animation.
 * current_frame - Index of the current frame being displayed.
 * tics_per_frame - Duration in whole local tics for each frame.
 * elapsed_tics - Accumulated time since the last frame change. Used in
 * `AdvanceSpriteAnimations()` to determine when to advance the animation (and
 * by how far, if applicable).
 */
struct SpriteAnimation {
    std::int32_t frame_width = 0;
    std::int32_t frame_height = 0;
    std::int32_t frame_count = 1;
    std::int32_t current_frame = 0;
    TicCount tics_per_frame = SecondsToTics(0.1);
    TicCount elapsed_tics = 0;
};

/**
 * Advances the sprite animations for all entities in the registry.
 *
 * Call once per simulation step, after AdvanceTimelines. Loops through entities
 * with SpriteAnimation, Sprite, and Timeline, incrementing elapsed_tics by the
 * timeline local delta. If the `elapsed_tics` exceeds the `tics_per_frame`, it
 * advances the animation to the next frame, wrapping around to the first frame
 * when necessary. The `elapsed_tics` is set to the excess time, so
 * `elapsed_tics` is always less than `tics_per_frame`.
 *
 * A rendered frame contains zero or one simulation steps. A Timeline retains
 * its most recent delta when merely read. Thus calling this on a frame without
 * a simulation step would apply an old delta again. Animations follow completed
 * simulation time, including its slowdown when rendering cannot keep up.
 *
 * In the case where multiple animation frames fit in one local delta, we
 * calculate the number of frames skipped based on `elapsed_tics` and
 * `tics_per_frame`, and advance accordingly. `elapsed_tics` is set to the
 * modulus, so it is always less than `tics_per_frame` as normal.
 * This can happen with a fast Timeline even if the source step is fixed to some
 * small value.
 *
 * @param registry The registry containing the entities and their components.
 */
void AdvanceSpriteAnimations(Registry &registry);

} // namespace svanes
