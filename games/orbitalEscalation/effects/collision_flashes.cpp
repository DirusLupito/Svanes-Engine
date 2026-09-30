#include "collision_flashes.hpp"

#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>
#include <svanes/timeline_system.hpp>

#include <algorithm>
#include <utility>

const svanes::TicCount kCollisionFlashLifetime = svanes::SecondsToTics(0.5);
const svanes::TicCount kCollisionFlashExpansionTime =
    svanes::SecondsToTics(0.05);
constexpr float kCollisionFlashInitialRadius = 4000.0F;
constexpr float kCollisionFlashExpansionRadius = 8000.0F;

void UpdateCollisionFlashes(svanes::Registry &world,
                            std::vector<OwnedEntity> &collision_flashes) {
    // if the flash has been alive for longer than its lifetime,
    // destroy it and remove it from the list of active flashes.
    std::erase_if(collision_flashes, [&](const OwnedEntity &owned) {
        const auto flash = owned.Get();
        const svanes::TicCount elapsed_tics =
            world.GetComponent<svanes::Timeline>(flash).GetTotalTics();
        if (elapsed_tics >= kCollisionFlashLifetime) {
            return true;
        }

        // Well we're already iterating over all the flashes,
        // so we might as well update their size and alpha here too.

        const float expansion =
            std::min(static_cast<float>(elapsed_tics) /
                         static_cast<float>(kCollisionFlashExpansionTime),
                     1.0F);
        const float fade =
            elapsed_tics <= kCollisionFlashExpansionTime
                ? 1.0F
                : 1.0F - static_cast<float>(elapsed_tics -
                                            kCollisionFlashExpansionTime) /
                             static_cast<float>(kCollisionFlashLifetime -
                                                kCollisionFlashExpansionTime);
        auto &gradient = world.GetComponent<svanes::RadialGradient2D>(flash);
        gradient.geometry.radius =
            std::lerp(kCollisionFlashInitialRadius,
                      kCollisionFlashExpansionRadius, expansion);
        gradient.center_color.alpha = static_cast<std::uint8_t>(255.0F * fade);
        return false;
    });
}

void CreateCollisionFlash(svanes::Registry &world,
                          svanes::Entity gameplay_timeline,
                          std::vector<OwnedEntity> &collision_flashes,
                          svanes::Vector2D contact_point) {

    OwnedEntity owned(world);
    const auto flash = owned.Get();
    world.AddComponent<svanes::Timeline>(flash, gameplay_timeline);
    world.AddComponent<svanes::ZOrder>(flash, svanes::ZOrder{100});
    world.AddComponent<svanes::Transform>(
        flash, svanes::Transform{contact_point.x, contact_point.y});

    // could probably have also worked with alpha blending and an
    // all white radial gradient.
    world.AddComponent<svanes::RadialGradient2D>(
        flash, svanes::RadialGradient2D{
                   .geometry = svanes::Circle2D{0.0F, 0.0F,
                                                kCollisionFlashInitialRadius},
                   .center_color = svanes::Color{255, 255, 255, 255},
                   .edge_color = svanes::Color{0, 0, 0, 0},
                   .blend_mode = svanes::BlendMode::Additive,
               });

    collision_flashes.push_back(std::move(owned));
}
