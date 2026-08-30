#include <svanes/sprite_animation_system.hpp>

#include <svanes/components/sprite_animation.hpp>
#include <svanes/components/transform.hpp>
#include <svanes/registry.hpp>

namespace svanes {

void AdvanceSpriteAnimations(Registry& registry, float delta_seconds)
{
    registry.ForEach<SpriteAnimation>([delta_seconds](Entity /*entity*/, SpriteAnimation& animation) {
        if (animation.frame_count <= 1 || animation.seconds_per_frame <= 0.0F) {
            return;
        }

        animation.elapsed_seconds += delta_seconds;
        while (animation.elapsed_seconds >= animation.seconds_per_frame) {
            animation.elapsed_seconds -= animation.seconds_per_frame;
            animation.current_frame = (animation.current_frame + 1) % animation.frame_count;
        }
    });
}

void RenderSprites(Registry& registry, SDL_Renderer* renderer)
{
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

} // namespace svanes
