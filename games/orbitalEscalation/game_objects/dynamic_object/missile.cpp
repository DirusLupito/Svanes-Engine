#include "missile.hpp"

#include "../../combat/damage_system.hpp"
#include "../../combat/missile_system.hpp"
#include <svanes/registry.hpp>

#include <cmath>
#include <stdexcept>
#include <utility>

Missile::Missile(svanes::Registry &world, svanes::Entity gameplay_timeline,
                 MissileDefinition definition)
    : DynamicObject(world, gameplay_timeline, std::move(definition.object)) {
    ValidateMissile(definition);
    world.AddComponent<Health>(GetEntity(), Health{definition.health});
    world.AddComponent<MissileExplosion>(
        GetEntity(),
        MissileExplosion{definition.damage, definition.blast_radius,
                         definition.arming_tics});
}

void ValidateMissile(const MissileDefinition &definition) {
    if (!std::isfinite(definition.health) || definition.health <= 0.0F ||
        !std::isfinite(definition.damage) || definition.damage < 0.0F ||
        !std::isfinite(definition.blast_radius) ||
        definition.blast_radius <= 0.0F) {
        throw std::invalid_argument(
            "Missile health and blast_radius must be finite and positive; "
            "damage must be finite and nonnegative");
    }
}
