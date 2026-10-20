#include "planet.hpp"

#include <svanes/registry.hpp>

#include <cmath>
#include <utility>

Planet::Planet(svanes::Registry &world, PlanetDefinition definition)
    : entity(world), visuals(world, std::move(definition.visuals)) {
    world.AddComponent<svanes::Transform>(entity.Get());
    world.AddComponent<svanes::Collider2D>(entity.Get(),
                                           std::move(definition.collider));
    world.AddComponent<svanes::PointAttractor2D>(
        entity.Get(), std::move(definition.attractor));
}

svanes::Entity Planet::GetEntity() const { return entity.Get(); }

svanes::Transform &Planet::GetTransform(svanes::Registry &world) const {
    return world.GetComponent<svanes::Transform>(entity.Get());
}

void Planet::UpdateVisuals(svanes::Registry &world) const {
    visuals.SetTransform(world, GetTransform(world));
}

// Controls the field strength of the planet's gravitational attraction.
constexpr float kPlanetAttraction = 180000000.0F;

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
        svanes::PerSecondSquaredToPerTicSquared(kPlanetAttraction) /
        (1.0F + distance * distance / OrbitalPlanetRadius);

    return offset_to_source / distance * strength;
}

/**
 * Creates a desert planet layer with a given radius, color, and z-order.
 *
 * @param radius The radius of the planet layer.
 * @param color The color of the planet layer.
 * @param z_order The z-order of the planet layer for rendering.
 *
 * @return The visual representing the planet layer.
 */
static Visual CreatePlanetLayer(float radius, svanes::Color color,
                                std::int32_t z_order) {
    return {svanes::SolidShape{color, svanes::Circle2D{0.0F, 0.0F, radius}},
            z_order};
}

PlanetDefinition Planet::MakeDefinition() {
    return PlanetDefinition{
        .attractor = {.accelerationField = AttractionField,
                      .cutoff_radius = std::nullopt,
                      .allow_parallel = true},
        .collider = {svanes::Circle2D{0.0F, 0.0F, OrbitalPlanetRadius}},
        .visuals = {CreatePlanetLayer(OrbitalPlanetRadius, {255, 127, 38, 255},
                                      -3),
                    CreatePlanetLayer(3900.0F, {185, 122, 87, 255}, -2),
                    CreatePlanetLayer(3750.0F, {127, 127, 127, 255}, -1)},
    };
}
