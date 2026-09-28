#include "ship.hpp"

#include <utility>

Ship::Ship(svanes::Registry &world, svanes::Entity gameplay_timeline,
           ShipDefinition definition)
    : DynamicObject(world, gameplay_timeline, std::move(definition.collider),
                    std::move(definition.visuals)),
      max_acceleration(definition.max_acceleration),
      max_angular_acceleration(definition.max_angular_acceleration) {}
