#include <svanes/sprite_animation_system.hpp>

#include <svanes/registry.hpp>

namespace svanes {


void AdvanceSpriteAnimations(Registry &registry) {
    // Only entities with a <SpriteAnimation>, a <Sprite>,
    // and a <Timeline> component need to be processed.
    registry.ForEach<SpriteAnimation, Sprite, Timeline>(
        [](Entity, SpriteAnimation &animation, Sprite &sprite,
           const Timeline &timeline) {
            if (animation.frame_count > 1 && animation.tics_per_frame > 0 &&
                timeline.GetDeltaTics() > 0) {

                // Accumulate the elapsed_tics so we can determine when to
                // advance the animation.
                animation.elapsed_tics += timeline.GetDeltaTics();

                // Do we need to advance the sprite animation?
                if (animation.elapsed_tics >= animation.tics_per_frame) {
                    // Move to the next frame.
                    if (animation.elapsed_tics < 2 * animation.tics_per_frame) {
                        animation.elapsed_tics -= animation.tics_per_frame;
                        animation.current_frame =
                            (animation.current_frame + 1) %
                            animation.frame_count;
                    }
                    // Calculate the next frame to move to.
                    // For example, if the Timeline runs fast enough to advance
                    // several animation frames within one simulation step.
                    else {
                        const TicCount num_frames = (animation.elapsed_tics /
                                                     animation.tics_per_frame) %
                                                    animation.frame_count;
                        animation.elapsed_tics %= animation.tics_per_frame;
                        animation.current_frame = static_cast<std::int32_t>(
                            (animation.current_frame + num_frames) %
                            animation.frame_count);
                    }
                }
            }

            // Update the sprite's source rectangle to reflect the current frame
            // of the animation so that in the next execution of
            // SubmitSprites(), the correct frame of the sprite sheet is drawn.
            sprite.source = Rectangle2D{
                static_cast<float>(animation.current_frame *
                                   animation.frame_width) +
                    animation.frame_width * 0.5F,
                animation.frame_height * 0.5F,
                static_cast<float>(animation.frame_width),
                static_cast<float>(animation.frame_height),
            };
        });
}

} // namespace svanes
