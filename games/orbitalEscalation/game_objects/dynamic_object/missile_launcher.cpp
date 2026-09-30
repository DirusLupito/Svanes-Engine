#include "missile_launcher.hpp"

#include "../../combat/weapon_system.hpp"
#include <svanes/registry.hpp>

#include <cmath>
#include <stdexcept>
#include <utility>

MissileLauncher::MissileLauncher(svanes::Registry &world,
                                 svanes::Entity gameplay_timeline,
                                 MissileLauncherDefinition definition)
    : DynamicObject(world, gameplay_timeline, std::move(definition.object)) {
    ValidateMissileLauncher(definition);

    // Ensure the forward direction is normalized to a unit vector.

    const float length = std::hypot(definition.forward.x, definition.forward.y);
    world.AddComponent<MissileLauncherState>(
        GetEntity(), MissileLauncherState{std::move(definition.missile),
                                          definition.reload_tics,
                                          definition.launch_speed,
                                          definition.forward / length,
                                          {},
                                          {}});

    world.AddComponent<WeaponControl>(GetEntity());
}

void ValidateMissileLauncher(const MissileLauncherDefinition &definition) {
    const float length = std::hypot(definition.forward.x, definition.forward.y);
    if (definition.missile.empty() || definition.reload_tics == 0 ||
        !std::isfinite(definition.launch_speed) ||
        definition.launch_speed < 0.0F || !std::isfinite(length) ||
        length <= 0.0F) {
        throw std::invalid_argument(
            "Launcher requires a missile name, positive reload time, finite "
            "nonnegative speed, and finite nonzero direction");
    }
}
