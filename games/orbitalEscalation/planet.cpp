#include "planet.hpp"

#include <svanes/attractor_system.hpp>
#include <svanes/collision_system.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>
#include <svanes/timeline_system.hpp>

#include <cmath>

constexpr float kPlanetRadius = 4200.0F;

/**
 * Helper for the planet's gravitational field.
 * Returns the acceleration vector at a given offset from the planet's center.
 *
 * @param offset_to_source The offset vector from the planet's center to the
 * point of interest.
 * @return The acceleration vector at the given offset, pointing towards the
 * planet's center.
 */
static svanes::Vector2D AttractionField(svanes::Vector2D offset_to_source) {
    const float distance = std::hypot(offset_to_source.x, offset_to_source.y);
    if (distance == 0.0F) {
        return {};
    }
    const float strength =
        svanes::PerSecondSquaredToPerTicSquared(180000000.0F) /
        (1.0F + distance * distance / kPlanetRadius);
    return offset_to_source / distance * strength;
}

/**
 * Creates a desert planet layer with a given radius, color, and z-order in the
 * provided registry.
 *
 * @param world The registry to create the planet layer in.
 * @param radius The radius of the planet layer.
 * @param color The color of the planet layer.
 * @param z_order The z-order of the planet layer for rendering.
 *
 * @return The entity representing the created planet layer.
 */
static svanes::Entity CreatePlanetLayer(svanes::Registry &world, float radius,
                                        svanes::Color color,
                                        std::int32_t z_order) {
    const svanes::Entity entity = world.CreateEntity();
    world.AddComponent<svanes::Transform>(entity);
    world.AddComponent<svanes::SolidShape>(
        entity,
        svanes::SolidShape{color, svanes::Circle2D{0.0F, 0.0F, radius}});
    world.AddComponent<svanes::ZOrder>(entity, svanes::ZOrder{z_order});
    return entity;
}

Planet::Planet(svanes::Registry &world)
    : layers{CreatePlanetLayer(world, kPlanetRadius, {255, 127, 38, 255}, -3),
             CreatePlanetLayer(world, 3900.0F, {185, 122, 87, 255}, -2),
             CreatePlanetLayer(world, 3750.0F, {127, 127, 127, 255}, -1)} {
    world.AddComponent<svanes::Collider2D>(
        GetEntity(),
        svanes::Collider2D{svanes::Circle2D{0.0F, 0.0F, kPlanetRadius}});
    world.AddComponent<svanes::PointAttractor2D>(
        GetEntity(),
        svanes::PointAttractor2D{.accelerationField = AttractionField,
                                 .cutoff_radius = std::nullopt,
                                 .allow_parallel = true});
}

svanes::Entity Planet::GetEntity() const { return layers.front(); }

float Planet::GetRadius() const { return kPlanetRadius; }
