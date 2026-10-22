#include "arena.hpp"

#include "goose.hpp"

#include <svanes/collision_system.hpp>
#include <svanes/kinematic_system.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>

#include <algorithm>
#include <array>
#include <vector>

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
constexpr float kDeathZoneReach = 2.0F;

constexpr std::array<float, 8> kSpawnX{400.0F,  550.0F,  700.0F,  850.0F,
                                       1000.0F, 1150.0F, 1300.0F, 1450.0F};

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

void CreateSpawnPoints(svanes::Registry &world) {
    const float spawn_y = kGroundTop - GooseHeight * 0.5F;

    for (const float spawn_x : kSpawnX) {
        const svanes::Entity entity = world.CreateEntity();
        world.AddComponent<svanes::Transform>(
            entity, svanes::Transform{.x = spawn_x, .y = spawn_y});
        world.AddComponent<SpawnPoint>(entity);
    }
}

void CreateDeathZones(svanes::Registry &world) {
    const float top = kGroundTop - kDeathZoneHeight;
    const svanes::Rectangle2D body{.width = kBorderThickness,
                                   .height = kGroundBottom - top};

    const svanes::Entity entity = world.CreateEntity();
    world.AddComponent<svanes::Transform>(
        entity,
        svanes::Transform{.x = kWorldLeft - kBorderThickness * 0.5F +
                               kDeathZoneReach,
                          .y = (top + kGroundBottom) * 0.5F});
    world.AddComponent<svanes::Collider2D>(entity, svanes::Collider2D{body});
    world.AddComponent<DeathZone>(entity);
}

bool ResolveSolidCollisions(svanes::Registry &world, svanes::Entity entity) {
    bool grounded = false;

    svanes::Transform &transform = world.GetComponent<svanes::Transform>(entity);
    svanes::Kinematic2D &motion =
        world.GetComponent<svanes::Kinematic2D>(entity);
    const svanes::Geometry2D body =
        world.GetComponent<svanes::Collider2D>(entity).geometry;

    world.ForEach<svanes::Transform, svanes::Collider2D, Solid>(
        [&](svanes::Entity, const svanes::Transform &other_transform,
            const svanes::Collider2D &other_collider, const Solid &) {
            const std::vector<svanes::Collision2D> collisions =
                svanes::DetectCollisions(body, transform,
                                         other_collider.geometry,
                                         other_transform);

            for (const svanes::Collision2D &collision : collisions) {
                transform.x += collision.normal.x * collision.penetration_depth;
                transform.y += collision.normal.y * collision.penetration_depth;

                if (collision.normal.y < 0.0F) {
                    grounded = true;
                    motion.velocity_y = std::min(motion.velocity_y, 0.0F);
                }

                if (collision.normal.y > 0.0F) {
                    motion.velocity_y = std::max(motion.velocity_y, 0.0F);
                }

                if (collision.normal.x != 0.0F) {
                    motion.velocity_x = 0.0F;
                }
            }
        });
    return grounded;
}
