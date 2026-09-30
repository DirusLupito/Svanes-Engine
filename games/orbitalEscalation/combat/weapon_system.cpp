#include "weapon_system.hpp"

#include "../attachment_system.hpp"
#include "../serialization/asset_catalog.hpp"
#include "damage_system.hpp"
#include "missile_system.hpp"
#include <svanes/registry.hpp>

#include <cmath>
#include <utility>

void SetWeaponControls(svanes::Registry &world, const DynamicObject &object,
                       WeaponControl control) {
    if (world.HasComponent<WeaponControl>(object.GetEntity())) {
        world.GetComponent<WeaponControl>(object.GetEntity()) = control;
    }

    // Attached objects can have their own launchers, so controls propagate
    // through the complete attachment tree.
    for (const auto &child : object.GetAttachments()) {
        SetWeaponControls(world, *child, control);
    }
}

/**
 * Updates one launcher, using its local timeline for the reload countdown.
 * The state is copied because creating a missile adds registry components.
 * We acquire the component again when writing the updated state back.
 *
 * @param world The registry containing the launcher.
 * @param object The launcher object that owns the loaded missile.
 * @param assets The catalog used to create replacement ammunition.
 * @param gameplay_timeline The parent timeline for new missiles.
 * @param projectiles The owners of fired missiles.
 */
static void
UpdateLauncher(svanes::Registry &world, DynamicObject &object,
               const AssetCatalog &assets, svanes::Entity gameplay_timeline,
               std::vector<std::unique_ptr<DynamicObject>> &projectiles) {
    const auto entity = object.GetEntity();
    const auto &timeline = world.GetComponent<svanes::Timeline>(entity);
    if (timeline.IsPaused() || timeline.GetDeltaTics() == 0) {
        return;
    }
    const auto now = timeline.GetTotalTics();
    auto state = world.GetComponent<MissileLauncherState>(entity);
    const bool fire =
        std::exchange(world.GetComponent<WeaponControl>(entity).fire, false);

    // Missiles can blow up while attached to the launcher, so we check for that
    // and clear the slot if necessary, restarting the reload countdown.
    if (state.loaded_missile &&
        (!world.HasComponent<Health>(*state.loaded_missile) ||
         IsDead(world, *state.loaded_missile))) {
        state.loaded_missile.reset();
        state.emptied_at = now;
    }

    // Reload the slot if it has been empty long enough.
    // TODO: Support loading more than one missile at a time.

    if (!state.loaded_missile && state.emptied_at &&
        now - *state.emptied_at >= state.reload_tics) {
        auto missile = assets.Create(world, gameplay_timeline,
                                     DynamicObjectType::Missile, state.missile);
        state.loaded_missile = missile->GetEntity();
        object.AddAttachment(world, std::move(missile), {});
        state.emptied_at.reset();
    }

    // Handle firing the missile (which can only happen if the slot is loaded).
    if (fire && state.loaded_missile) {
        const auto pose = object.GetTransform(world);
        const float cosine = std::cos(pose.rotation);
        const float sine = std::sin(pose.rotation);

        const svanes::Vector2D direction{
            state.forward.x * cosine - state.forward.y * sine,
            state.forward.x * sine + state.forward.y * cosine};

        const auto firing_ship = GetAttachmentRoot(world, entity);
        auto missile = object.Detach(
            world, *state.loaded_missile,
            direction * svanes::PerSecondToPerTic(state.launch_speed));

        // Record the firing ship and launch time so the missile can ignore its
        // source while it is still arming.
        auto &explosion =
            world.GetComponent<MissileExplosion>(missile->GetEntity());
        explosion.firing_ship = firing_ship;

        explosion.launched_at =
            world.GetComponent<svanes::Timeline>(missile->GetEntity())
                .GetTotalTics();

        projectiles.push_back(std::move(missile));
        state.loaded_missile.reset();
        state.emptied_at = now;
    }

    world.GetComponent<MissileLauncherState>(entity) = std::move(state);
}

void UpdateWeapons(svanes::Registry &world, DynamicObject &object,
                   const AssetCatalog &assets, svanes::Entity gameplay_timeline,
                   std::vector<std::unique_ptr<DynamicObject>> &projectiles) {
    if (IsDead(world, object.GetEntity())) {
        return;
    }

    if (world.HasComponent<MissileLauncherState>(object.GetEntity())) {
        UpdateLauncher(world, object, assets, gameplay_timeline, projectiles);
    }

    // what if a launcher is attached to a launcher?
    for (const auto &child : object.GetAttachments()) {
        UpdateWeapons(world, *child, assets, gameplay_timeline, projectiles);
    }
}
