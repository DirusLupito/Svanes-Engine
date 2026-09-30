#include "missile.hpp"

#include <utility>

Missile::Missile(svanes::Registry &world, svanes::Entity gameplay_timeline,
                 MissileDefinition definition)
    : DynamicObject(world, gameplay_timeline, std::move(definition.object)) {}
