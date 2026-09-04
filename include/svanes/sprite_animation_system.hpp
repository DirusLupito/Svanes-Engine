#pragma once

#include <cstdint>

#include <svanes/render/render_system.hpp>

namespace svanes {

class Registry;

/**
 * Represents a sprite animation in a 2D game.
 *
 * frame_width - Width of each frame in the sprite sheet.
 * frame_height - Height of each frame in the sprite sheet.
 * frame_count - Total number of frames in the animation.
 * current_frame - Index of the current frame being displayed.
 * seconds_per_frame - Duration in seconds for each frame.
 * elapsed_seconds - Accumulated time since the last frame change. Used in `AdvanceSpriteAnimations()` to determine when to advance the animation (and by how far, if applicable).
 */
struct SpriteAnimation {
    std::int32_t frame_width = 0;
    std::int32_t frame_height = 0;
    std::int32_t frame_count = 1;
    std::int32_t current_frame = 0;
    float seconds_per_frame = 0.1F;
    float elapsed_seconds = 0.0F;
};

/**
 * Advances the sprite animations for all entities in the registry.
 * 
 * Called once per frame, passing in the elapsed time since the last frame passed in as `delta_seconds`.
 * Loops through all entities that have both a <SpriteAnimation> and <Sprite> component, and increments the `elapsed_seconds`
 * by `delta_seconds`. If the `elapsed_seconds` exceeds the `seconds_per_frame`, it advances the animation 
 * to the next frame, wrapping around to the first frame when necessary. The `elapsed_seconds` is set to the
 * excess time, so `elapsed_seconds` is always less than `seconds_per_frame`.
 * 
 * In the case where multiple frames need to be skipped due to lag spikes, we calculate the number of frames
 * skipped based on `elapsed_seconds` and `seconds_per_frame`, and advance accordingly. `elapsed_seconds` is
 * set to the modulus, so it is always less than `seconds_per_frame` as normal.
 *
 * @param registry The registry containing the entities and their components.
 * @param delta_seconds The time elapsed since the last frame.
 */
void AdvanceSpriteAnimations(Registry& registry, float delta_seconds);

}
