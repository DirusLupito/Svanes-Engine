#pragma once

#include <svanes/entity.hpp>

#include <optional>
#include <unordered_set>

namespace svanes {
class CollisionEvent;
class EventManager;
class Registry;
} // namespace svanes

/**
 * Registers a collision handler that pushes colliding objects apart by
 * using the contact normals, provided the objects qualify for collision
 * physics. Dispatch collisions in the same physics step before removing
 * the affected entities.
 *
 * @param world The registry containing the colliding objects. It must outlive
 * the registration.
 * @param events The event manager receiving collision events for this world.
 *
 * @throws std::invalid_argument if this handler is already registered.
 * @throws std::logic_error if called during an event callback.
 */
void RegisterPhysicalCollisionHandler(svanes::Registry &world,
                                      svanes::EventManager &events);

/**
 * Handles ship collisions with the planet, applying impact damage when contact
 * begins. Tracks contacts across physics steps so remaining on the surface
 * does not repeatedly damage a ship.
 */
class PlanetCollisionDamageHandler final {
public:
    /**
     * Registers the planet collision damage handler to receive collision events for the world.
     *
     * @param world The registry containing the ships and planet.
     * @param events The event manager receiving collisions for this world.
     *
     * @throws std::invalid_argument if this handler type is already registered.
     * @throws std::logic_error if called during an event callback.
     */
    void Register(svanes::Registry &world, svanes::EventManager &events);

    /**
     * Starts tracking contacts for a new physics step. Call once before each
     * collision pass, including passes with no collisions, so a ship can take
     * impact damage again after leaving the surface.
     *
     * @param planet The planet entity in this world's registry.
     */
    void BeginStep(svanes::Entity planet);

private:
    /**
     * Applies impact damage to a living ship meeting the planet for the first
     * time since it last left the surface.
     *
     * @param world The registry containing the colliding entities.
     * @param event The collision to check for ship contact with the planet.
     */
    void HandleCollision(svanes::Registry &world,
                         const svanes::CollisionEvent &event);

    // The planet receiving ship contacts, set by BeginStep.
    std::optional<svanes::Entity> planet;

    // Ships that touched the planet during the previous physics step.
    std::unordered_set<svanes::Entity> previous_contacts;

    // Ships already recorded this step, so multiple contacts cause one impact.
    std::unordered_set<svanes::Entity> current_contacts;
};
