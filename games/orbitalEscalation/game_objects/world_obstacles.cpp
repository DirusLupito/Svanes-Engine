#include "world_obstacles.hpp"

#include "planet.hpp"

#include <svanes/kinematic_system.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>
#include <svanes/timeline_system.hpp>

#include <cmath>
#include <numbers>
#include <utility>

// The radius of the circle enclosing either obstacle.
constexpr float kObstacleRadius = OrbitalPlanetRadius;

// The distance from the planet's center to either obstacle's center.
constexpr float kObstacleDistance = 8.0F * OrbitalPlanetRadius;

/**
 * Creates a fixed obstacle with matching collision geometry and appearance.
 *
 * @param world The registry in which to create the obstacle.
 * @param geometry The shape used for both collision and rendering.
 * @param pose The obstacle's position and initial rotation.
 * @param color The obstacle's solid color.
 *
 * @return The owner of the obstacle's entity.
 */
static OwnedEntity CreateObstacle(svanes::Registry &world,
                                  svanes::Geometry2D geometry,
                                  svanes::Transform pose, svanes::Color color) {
    OwnedEntity entity(world);
    world.AddComponent<svanes::Transform>(entity.Get(), pose);
    world.AddComponent<svanes::Collider2D>(entity.Get(),
                                           svanes::Collider2D{geometry});
    world.AddComponent<svanes::SolidShape>(
        entity.Get(), svanes::SolidShape{color, std::move(geometry)});
    return entity;
}

OwnedEntity CreateRepellingTriangle(svanes::Registry &world) {

    // triangle is green, stationary, and emits a repulsion field.

    const svanes::Triangle2D geometry{
        {svanes::Vector2D{0.0F, -kObstacleRadius},
         svanes::Vector2D{std::sqrt(3.0F) * kObstacleRadius * 0.5F,
                          kObstacleRadius * 0.5F},
         svanes::Vector2D{-std::sqrt(3.0F) * kObstacleRadius * 0.5F,
                          kObstacleRadius * 0.5F}}};

    auto triangle = CreateObstacle(world, geometry, {-kObstacleDistance, 0.0F},
                                   {0, 255, 0, 255});

    // the triangle is just emitting a slightly modified version of the planet's
    // attractor field, but with a cutoff radius and reversed direction.
    auto repulsion = Planet::MakeDefinition().attractor;
    repulsion.accelerationField = [attraction =
                                       std::move(repulsion.accelerationField)](
                                      svanes::Vector2D offset_to_source) {
        return attraction(offset_to_source) * -1.0F;
    };

    repulsion.cutoff_radius = kObstacleRadius;
    world.AddComponent<svanes::PointAttractor2D>(triangle.Get(),
                                                 std::move(repulsion));

    return triangle;
}

OwnedEntity CreateRotatingSquare(svanes::Registry &world,
                                 svanes::Entity gameplay_timeline) {

    // square is just gray and rotates in place.

    const float side = std::sqrt(2.0F) * kObstacleRadius;
    auto square =
        CreateObstacle(world, svanes::Rectangle2D{0.0F, 0.0F, side, side},
                       {kObstacleDistance, 0.0F}, {127, 127, 127, 255});
    world.AddComponent<svanes::Timeline>(square.Get(), gameplay_timeline);
    world.AddComponent<svanes::Kinematic2D>(
        square.Get(),
        svanes::Kinematic2D{.angular_velocity = svanes::PerSecondToPerTic(
                                2.0F * std::numbers::pi_v<float> / 30.0F),
                            .max_speed = 0.0F,
                            .max_acceleration = 0.0F});
    return square;
}
