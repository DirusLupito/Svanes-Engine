#pragma once

namespace svanes {
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
