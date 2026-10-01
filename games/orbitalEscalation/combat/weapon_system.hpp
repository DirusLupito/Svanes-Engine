#pragma once

#include "../game_objects/dynamic_object/dynamic_object.hpp"
#include <optional>
#include <svanes/timeline_system.hpp>

class AssetCatalog;

/**
 * Requests a firing action, which is separate from the weapon's decision to
 * fire. The weapon system consumes the request, and decides whether it has
 * ammunition and is ready to fire.
 *
 * FIELDS:
 * - fire: Whether to attempt one shot. The weapon system consumes the request,
 * even if the weapon is empty.
 */
struct WeaponControl {
    bool fire = false;
};

/**
 * Stores the state of a missile launcher. There is only one ammunition
 * slot, and the missile is an actual attached object, not just a counter.
 *
 * TODO: Support multiple ammunition slots, for multishot launchers.
 *
 * FIELDS:
 * - missile: The name of the missile definition to load.
 * - reload_tics: The local time required to replace an emptied slot.
 * - launch_speed: The additional launch speed to be added to the ship's
 * velocity when firing a missile.
 * - forward: The launch direction in the launcher's local coordinates.
 * - loaded_missile: The entity in the ammunition slot, or no entity if empty.
 * - ammunition_lost: Whether destroyed ammunition still needs to start the
 * reload countdown on the next active weapon update.
 * - emptied_at: The launcher's total local tics when the slot became empty.
 */
struct MissileLauncherState {
    std::string missile;
    svanes::TicCount reload_tics;
    float launch_speed;
    svanes::Vector2D forward;
    std::optional<svanes::Entity> loaded_missile;
    std::optional<svanes::TicCount> emptied_at;
    bool ammunition_lost = false;
};

/**
 * Sets the weapon controls of an object and its attachments.
 *
 * @param world The registry containing the weapon controls.
 * @param object The object whose weapons should receive the controls.
 * @param control The requested weapon controls.
 */
void SetWeaponControls(svanes::Registry &world, const DynamicObject &object,
                       WeaponControl control);

/**
 * Reloads and fires weapons on an object and its attachments. A fired missile
 * keeps its attachment position and inherits the ship's velocity plus the
 * configured launch speed. Its arming time starts here when it is fired.
 *
 * @param world The registry containing the objects.
 * @param object The object whose weapons should be updated.
 * @param assets The catalog used to create replacement ammunition.
 * @param gameplay_timeline The parent timeline for new missiles.
 * @param projectiles The owners of fired missiles, independent of the ship.
 */
void UpdateWeapons(svanes::Registry &world, DynamicObject &object,
                   const AssetCatalog &assets, svanes::Entity gameplay_timeline,
                   std::vector<std::unique_ptr<DynamicObject>> &projectiles);
