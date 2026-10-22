#include "arena.hpp"

#include "goose.hpp"

#include <svanes/collision_system.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>

#include <array>

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

constexpr float kDeathZoneHeight = 5.0F * GooseHeight;
constexpr svanes::Color kDeathZoneColor{.red = 200, .green = 30, .blue = 30};

constexpr std::array<float, 8> kSpawnX{400.0F,  550.0F,  700.0F,  850.0F,
                                       1000.0F, 1150.0F, 1300.0F, 1450.0F};
constexpr float kSpawnOutlineThickness = 3.0F;
constexpr svanes::Color kSpawnColor{.red = 170, .green = 80, .blue = 220};

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

/**
 * Creates a death zone, drawn as a colored rectangle and tagged DeathZone so
 * geese touching it are killed.
 *
 * @param world The registry the entity is created in.
 * @param center_x The world x position of the zone's center.
 * @param center_y The world y position of the zone's center.
 * @param width The zone's width.
 * @param height The zone's height.
 */
void CreateDeathZone(svanes::Registry &world, float center_x, float center_y,
                     float width, float height) {
    const svanes::Rectangle2D body{.width = width, .height = height};

    const svanes::Entity entity = world.CreateEntity();
    world.AddComponent<svanes::Transform>(
        entity, svanes::Transform{.x = center_x, .y = center_y});
    world.AddComponent<svanes::SolidShape>(
        entity, svanes::SolidShape{.color = kDeathZoneColor, .geometry = body});
    world.AddComponent<svanes::Collider2D>(entity, svanes::Collider2D{body});
    world.AddComponent<DeathZone>(entity);
}

/**
 * @return A goose sized rectangular outline, built from four thin bars around
 * an empty middle.
 */
svanes::CompositeShape2D SpawnOutline() {
    const float half_width = GooseWidth * 0.5F;
    const float half_height = GooseHeight * 0.5F;
    const svanes::Rectangle2D horizontal{.width = GooseWidth,
                                         .height = kSpawnOutlineThickness};
    const svanes::Rectangle2D vertical{.width = kSpawnOutlineThickness,
                                       .height = GooseHeight};

    return svanes::CompositeShape2D{{
        {horizontal, svanes::Transform{.y = -half_height}},
        {horizontal, svanes::Transform{.y = half_height}},
        {vertical, svanes::Transform{.x = -half_width}},
        {vertical, svanes::Transform{.x = half_width}},
    }};
}

} // namespace

void CreateArena(svanes::Registry &world) {
    const float world_width = kWorldRight - kWorldLeft;
    const float world_center_x = (kWorldLeft + kWorldRight) * 0.5F;
    const float border_center_y = (kWorldTop + kGroundBottom) * 0.5F;
    const float border_height = kGroundBottom - kWorldTop;
    const float left_wall_x = kWorldLeft - kBorderThickness * 0.5F;
    const float death_zone_top = kGroundTop - kDeathZoneHeight;

    // TASK 2A, static entity: the ground. A Transform, a SolidShape to draw, a
    // Collider2D to be hit, and the Solid tag, with no motion components at all, so
    // nothing the engine does each frame can move it.
    CreateSolidBlock(world, world_center_x, (kGroundTop + kGroundBottom) * 0.5F,
                     world_width, kGroundBottom - kGroundTop, kGroundColor);

    // the left, right and top walls, built the same way, closing off the arena
    CreateSolidBlock(world, left_wall_x, (kWorldTop + death_zone_top) * 0.5F,
                     kBorderThickness, death_zone_top - kWorldTop, kWallColor);
    CreateDeathZone(world, left_wall_x, (death_zone_top + kGroundBottom) * 0.5F,
                    kBorderThickness, kGroundBottom - death_zone_top);

    CreateSolidBlock(world, kWorldRight + kBorderThickness * 0.5F,
                     border_center_y, kBorderThickness, border_height,
                     kWallColor);

    CreateSolidBlock(world, world_center_x, kWorldTop - kBorderThickness * 0.5F,
                     world_width + 2.0F * kBorderThickness, kBorderThickness,
                     kWallColor);
}

void CreateSpawnPoints(svanes::Registry &world) {
    const float spawn_y = kGroundTop - GooseHeight * 0.5F;

    for (const float spawn_x : kSpawnX) {
        const svanes::Entity entity = world.CreateEntity();
        world.AddComponent<svanes::Transform>(
            entity, svanes::Transform{.x = spawn_x, .y = spawn_y});
        world.AddComponent<svanes::SolidShape>(
            entity,
            svanes::SolidShape{.color = kSpawnColor, .geometry = SpawnOutline()});
        world.AddComponent<SpawnPoint>(entity);
    }
}
