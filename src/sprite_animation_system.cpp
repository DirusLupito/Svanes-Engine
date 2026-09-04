#include <svanes/sprite_animation_system.hpp>

#include <cmath>

#include <svanes/registry.hpp>

namespace svanes {


void AdvanceSpriteAnimations(Registry& registry, float delta_seconds)
{
    // Only entities with both a <SpriteAnimation> and <Sprite> component need to be processed.
    registry.ForEach<SpriteAnimation, Sprite>([delta_seconds](Entity, SpriteAnimation& animation, Sprite& sprite) {
        if (animation.frame_count > 1 && animation.seconds_per_frame > 0.0F) {
            // Accumulate the elapsed_seconds so we can determine when to advance the animation.
            animation.elapsed_seconds += delta_seconds;

            // Do we need to advance the sprite animation?
            if (animation.elapsed_seconds >= animation.seconds_per_frame) {
                // Move to the next frame.
                if (animation.elapsed_seconds < 2.0F * animation.seconds_per_frame) {
                    animation.elapsed_seconds -= animation.seconds_per_frame;
                    animation.current_frame = (animation.current_frame + 1) % animation.frame_count;
                }
                // Calculate the next frame to move to.
                // Realistically, only if there's a lag spike.
                else {
                    const std::int32_t num_frames = static_cast<std::int32_t>(std::trunc(animation.elapsed_seconds / animation.seconds_per_frame));
                    animation.elapsed_seconds = std::fmod(animation.elapsed_seconds, animation.seconds_per_frame);
                    animation.current_frame = (animation.current_frame + num_frames) % animation.frame_count;
                }
            }
        }

        // Update the sprite's source rectangle to reflect the current frame of the animation
        // so that in the next execution of SubmitSprites(), the correct frame of the sprite sheet is drawn.
        sprite.source = Rectangle{
            static_cast<float>(animation.current_frame * animation.frame_width),
            0.0F,
            static_cast<float>(animation.frame_width),
            static_cast<float>(animation.frame_height),
        };
    });
}

}
