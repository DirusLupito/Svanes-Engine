#include <svanes/registry.hpp>

namespace svanes {

Entity Registry::CreateEntity()
{
    return next_entity++;
}

void Registry::DestroyEntity(Entity entity)
{
    components.erase(entity);
}

} // namespace svanes
