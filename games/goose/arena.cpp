#include "arena.hpp"

#include <svanes/collision_system.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>

namespace {

// the playable area, enclosed by the ground below and border walls on the other
// three sides
constexpr float kWorldLeft = 0.0F;
constexpr float kWorldRight = 6000.0F;
constexpr float kWorldTop = -1200.0F;
constexpr float kGroundTop = 974.0F;
constexpr float kGroundBottom = 1080.0F;
constexpr float kBorderThickness = 100.0F;

constexpr svanes::Color kGroundColor{.red = 60, .green = 140, .blue = 70};
constexpr svanes::Color kWallColor{.red = 90, .green = 90, .blue = 110};

/**
 * Creates one piece of immovable world geometry, drawn as a colored rectangle
 * and tagged Solid so geese and bullets collide with it.
 *
 * @param world The registry the entity is created in.
 * @param center_x The world x position of the block's center.
 * @param center_y The world y position of the block's center.
 * @param width The block's width.
 * @param height The block's height.
 * @param color The color the block is drawn in.
 */
void CreateSolidBlock(svanes::Registry &world, float center_x, float center_y,
                      float width, float height, svanes::Color color) {
    const svanes::Rectangle2D body{.width = width, .height = height};

    const svanes::Entity entity = world.CreateEntity();
    world.AddComponent<svanes::Transform>(
        entity, svanes::Transform{.x = center_x, .y = center_y});
    world.AddComponent<svanes::SolidShape>(
        entity, svanes::SolidShape{.color = color, .geometry = body});
    world.AddComponent<svanes::Collider2D>(entity, svanes::Collider2D{body});
    world.AddComponent<Solid>(entity);
}

} // namespace

void CreateArena(svanes::Registry &world) {
    const float world_width = kWorldRight - kWorldLeft;
    const float world_center_x = (kWorldLeft + kWorldRight) * 0.5F;
    const float border_center_y = (kWorldTop + kGroundBottom) * 0.5F;
    const float border_height = kGroundBottom - kWorldTop;

    // TASK 2A, static entity: the ground. A Transform, a SolidShape to draw, a
    // Collider2D to be hit, and the Solid tag, with no motion components at all, so
    // nothing the engine does each frame can move it.
    CreateSolidBlock(world, world_center_x, (kGroundTop + kGroundBottom) * 0.5F,
                     world_width, kGroundBottom - kGroundTop, kGroundColor);

    // the left, right and top walls, built the same way, closing off the arena
    CreateSolidBlock(world, kWorldLeft - kBorderThickness * 0.5F,
                     border_center_y, kBorderThickness, border_height,
                     kWallColor);

    CreateSolidBlock(world, kWorldRight + kBorderThickness * 0.5F,
                     border_center_y, kBorderThickness, border_height,
                     kWallColor);

    CreateSolidBlock(world, world_center_x, kWorldTop - kBorderThickness * 0.5F,
                     world_width + 2.0F * kBorderThickness, kBorderThickness,
                     kWallColor);
}
