#include <svanes/sprite_animation_system.hpp>

#include <cmath>

#include <svanes/registry.hpp>

namespace svanes {

/**
 * Advances the sprite animations for all entities in the registry.
 * 
 * Called once per frame, passing in the elapsed time since the last frame passed in as `delta_seconds`.
 * Loops through all entities that have a <SpriteAnimation> component, and increments the `elapsed_seconds` 
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
void AdvanceSpriteAnimations(Registry& registry, float delta_seconds)
{
    // Run a ForEach on our registry to get every entity that has a <SpriteAnimation> component.
    registry.ForEach<SpriteAnimation>([delta_seconds](Entity /*entity*/, SpriteAnimation& animation) {
        // Return early when there is only one frame or there are '0' seconds per frame.
        if (animation.frame_count <= 1 || animation.seconds_per_frame <= 0.0F) {
            return;
        }

        // Accumulate the elapsed_seconds.
        animation.elapsed_seconds += delta_seconds;
        
        // Do we need to advance the sprite animation?
        if (animation.elapsed_seconds >= animation.seconds_per_frame) {
            // Move to the next frame.
            if (animation.elapsed_seconds < 2.0f * animation.seconds_per_frame) {
                animation.elapsed_seconds -= animation.seconds_per_frame;
                animation.current_frame = (animation.current_frame + 1) % animation.frame_count;
            }
            // Calculate the next frame to move to.
            // Realistically, only if there's a lag spike.
            else {
                int num_frames = static_cast<int>(std::trunc(animation.elapsed_seconds / animation.seconds_per_frame));
                animation.elapsed_seconds = std::fmod(animation.elapsed_seconds, animation.seconds_per_frame);
                animation.current_frame = (animation.current_frame + num_frames) % animation.frame_count;
            }
        }
    });
}

void RenderSprites(Registry& registry, SDL_Renderer* renderer)
{
    // Run a ForEach on our registry, only including entities that have both a <SpriteAnimation> and <Transform> component.
    // 
    // When running a ForEach<> on our registry, the order that the components are presented in the ForEach is important.
    // We iterate the pool of the first listed component type, and filter by the rest. Here, it is faster to first iterate 
    // only over the components with <SpriteAnimation>, and then only take those that have a <Transform> component. This 
    // is because there are likely a less amount of entities to have a <SpriteAnimation> component than amount of entities 
    // that have a <Transform> component.
    registry.ForEach<SpriteAnimation, Transform>([renderer](Entity /*entity*/, SpriteAnimation& animation, Transform& transform) {
        if (animation.texture == nullptr) {
            return;
        }

        const SDL_FRect source{
            static_cast<float>(animation.current_frame * animation.frame_width),
            0.0F,
            static_cast<float>(animation.frame_width),
            static_cast<float>(animation.frame_height),
        };

        const SDL_FRect destination{
            transform.x,
            transform.y,
            static_cast<float>(animation.frame_width),
            static_cast<float>(animation.frame_height),
        };

        SDL_RenderTexture(renderer, animation.texture, &source, &destination);
    });
}

}
