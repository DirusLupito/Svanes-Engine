#pragma once

#include "dynamic_object.hpp"
#include <svanes/timeline_system.hpp>

/**
 * Defines a single-missile launcher. It starts loaded, and replaces its missile
 * after its own timeline has advanced by reload_tics since becoming empty.
 *
 * TODO: Support multiple missiles in a single launcher.
 *
 * FIELDS:
 * - object: The launcher's name, collider, visuals, and attachment definitions.
 * - missile: The name of the missile definition used for ammunition.
 * - reload_tics: The local time required to reload, in tics.
 * - launch_speed: The speed at which the missile is launched relative to the
 * launcher, in units per tic.
 * - forward: The launch direction in local coordinates. It is normalized when
 * the launcher is constructed.
 */
struct MissileLauncherDefinition {
    DynamicObjectDefinition object;
    std::string missile;
    svanes::TicCount reload_tics;
    float launch_speed;
    svanes::Vector2D forward;
};

/**
 * Represents a single-missile launcher in the game world. The launcher stays
 * attached to the ship when fired; only its loaded missile is detached.
 */
class MissileLauncher final : public DynamicObject {
public:
    /**
     * Constructs a launcher with its weapon controls and reload state. The
     * catalog supplies its initial missile after constructing the launcher.
     *
     * @param world The registry in which to create the launcher.
     * @param gameplay_timeline The parent timeline for the launcher.
     * @param definition The launcher's geometry and weapon properties.
     */
    MissileLauncher(svanes::Registry &world, svanes::Entity gameplay_timeline,
                    MissileLauncherDefinition definition);
};

/**
 * Validates a launcher's reload time, launch speed, and launch direction.
 *
 * @param definition The launcher definition to validate.
 * @throws std::invalid_argument If the missile name is empty, reload time is
 * zero, speed is not finite and nonnegative, or direction has invalid length.
 */
void ValidateMissileLauncher(const MissileLauncherDefinition &definition);
