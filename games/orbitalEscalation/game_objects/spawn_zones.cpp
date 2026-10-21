#include "spawn_zones.hpp"

#include "../combat/damage_system.hpp"
#include "dynamic_object/dynamic_object.hpp"
#include "planet.hpp"

#include <svanes/registry.hpp>

// The number of spawn zones surrounding the origin.
constexpr std::uint32_t kSpawnZoneCount = 8;

// The distance between adjacent rows or columns of spawn zones.
constexpr float kSpawnGridSpacing = 5.0F * OrbitalPlanetRadius;

// The radius of each spawn zone. Should be large enough to cover a player ship.
constexpr float kSpawnZoneRadius = 400.0F;

SpawnZones::SpawnZones(svanes::Registry &world) {
    // We construct the spawn zones in a 3x3 grid around the origin, leaving the
    // center empty. Each zone is represented by a circular collider.
    for (const float y : {-kSpawnGridSpacing, 0.0F, kSpawnGridSpacing}) {
        for (const float x : {-kSpawnGridSpacing, 0.0F, kSpawnGridSpacing}) {
            if (x == 0.0F && y == 0.0F) {
                continue;
            }

            const auto entity = zones.emplace_back(world).Get();
            world.AddComponent<svanes::Collider2D>(
                entity,
                svanes::Collider2D{svanes::Circle2D{x, y, kSpawnZoneRadius}});
        }
    }
}

/**
 * Checks whether a player's ship or any of its living attached parts
 * overlaps a spawn zone.
 *
 * @param world The registry containing the player ship.
 * @param object The ship or attached part to check.
 * @param zone The circular spawn area in world coordinates.
 * @return Whether the object or any living attachment occupies the zone.
 */
static bool OccupiesSpawnZone(const svanes::Registry &world,
                              const DynamicObject &object,
                              const svanes::Collider2D &zone) {
    const auto entity = object.GetEntity();
    if (IsDead(world, entity)) {
        return false;
    }

    const auto &geometry =
        world.GetComponent<svanes::Collider2D>(entity).geometry;

    const auto &transform = world.GetComponent<svanes::Transform>(entity);
    if (!svanes::DetectCollisions(geometry, transform, zone.geometry, {})
             .empty()) {
        return true;
    }

    for (const auto &child : object.GetAttachments()) {
        if (OccupiesSpawnZone(world, *child, zone)) {
            return true;
        }
    }
    return false;
}

svanes::Vector2D
SpawnZones::ChoosePosition(const svanes::Registry &world,
                           std::span<const DynamicObject *const> player_ships) {
    // Try each zone once, then force a spawn at the following zone if none
    // were free. Even an occupied zone advances the next choice.
    for (std::uint32_t attempt = 0;; ++attempt) {
        const auto &zone =
            world.GetComponent<svanes::Collider2D>(zones[next_spawn].Get());
        next_spawn = (next_spawn + 1) % kSpawnZoneCount;

        bool occupied = false;
        if (attempt < kSpawnZoneCount) {

            // Rather than call DetectEntityCollisions, we instead
            // check each object individually to see if it occupies the spawn
            // zone. Will probably be faster because DetectEntityCollisions
            // would detect all collisions between all objects, which we don't
            // need (we only care about collisions with the spawn zone). But if
            // this proves to be really slow we still shouldn't call
            // DetectEntityCollisions, instead, we should implement a more
            // specialized broad-phase collision detection that only checks for
            // collisions between one object and a set of other objects.
            for (const auto *ship : player_ships) {
                if (OccupiesSpawnZone(world, *ship, zone)) {
                    occupied = true;
                    break;
                }
            }
        }

        if (occupied) {
            continue;
        }

        const auto &circle = std::get<svanes::Circle2D>(zone.geometry);
        return {circle.x, circle.y};
    }
}
