#include <svanes/collision_system.hpp>

#include <svanes/render/render_system.hpp>

#include <SDL3/SDL.h>

namespace svanes {

/**
 * Helper to convert an entitys transform into a rectangle for overlap calculations.
 *
 * transform - The transform of the entity to be conveerted
 */
static SDL_FRect ToRect(const Transform& transform)
{
    return SDL_FRect{transform.x, transform.y, transform.width, transform.height};
}

std::optional<Rectangle> GetOverlap(const Transform& a, const Transform& b)
{
    const SDL_FRect rect_a = ToRect(a);
    const SDL_FRect rect_b = ToRect(b);
    SDL_FRect overlap{};

    if (!SDL_GetRectIntersectionFloat(&rect_a, &rect_b, &overlap)) {
        return std::nullopt;
    }

    if (overlap.w <= 0.0F || overlap.h <= 0.0F) {
        return std::nullopt;
    }

    return Rectangle{overlap.x, overlap.y, overlap.w, overlap.h};
}

} // namespace svanes
