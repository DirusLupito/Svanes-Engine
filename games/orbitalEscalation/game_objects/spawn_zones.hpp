#pragma once

#include "owned_entity.hpp"

#include <svanes/vector2d.hpp>

#include <cstdint>
#include <span>
#include <vector>

class DynamicObject;

/**
 * Owns Orbital Escalation's eight invisible circular spawn zones and cycles
 * through them to choose positions clear of ships and missiles.
 */
class SpawnZones final {
public:
    /**
     * Creates eight spawn zones in a grid surrounding the origin, leaving
     * the center empty.
     *
     * @param world The registry in which to create the spawn zones.
     */
    explicit SpawnZones(svanes::Registry &world);

    /**
     * Chooses the next zone not occupied by a living ship, missile, or its
     * attachments. Every checked zone advances the choice to the next one.
     * If all eight are occupied, the following zone is used anyway.
     *
     * @param world The registry containing the spawn zones and objects.
     * @param objects The nonnull ships and missiles to check for occupancy,
     * including their attachments.
     * @return The world position at the center of the chosen zone.
     */
    svanes::Vector2D
    ChoosePosition(const svanes::Registry &world,
                   std::span<const DynamicObject *const> objects);

private:
    // List of the circular entities that define the spawn zones.
    std::vector<OwnedEntity> zones;

    // The index of the next zone to try, advanced after every spawn attempt.
    std::uint32_t next_spawn = 0;
};
