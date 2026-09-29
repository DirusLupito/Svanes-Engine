#include "damage_system.hpp"

#include <svanes/registry.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

bool ApplyDamage(svanes::Registry &world, svanes::Entity target, float amount) {
    if (!std::isfinite(amount) || amount < 0.0F) {
        throw std::invalid_argument("Damage must be finite and nonnegative.");
    }
    if (!world.HasComponent<Health>(target)) {
        return false;
    }

    auto &health = world.GetComponent<Health>(target);
    health.remaining = std::max(0.0F, health.remaining - amount);
    return health.remaining == 0.0F;
}
