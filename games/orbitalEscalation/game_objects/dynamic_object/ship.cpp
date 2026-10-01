#include "ship.hpp"

#include "../../propulsion_system.hpp"

#include <svanes/registry.hpp>

#include <utility>

Ship::Ship(svanes::Registry &world, svanes::Entity gameplay_timeline,
           ShipDefinition definition)
    : DynamicObject(world, gameplay_timeline, std::move(definition.object)) {
    world.AddComponent<Propulsion>(GetEntity(), definition.max_acceleration,
                                   definition.max_angular_acceleration,
                                   definition.forward);
    world.AddComponent<PropulsionControl>(GetEntity());
}
