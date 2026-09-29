#include <svanes/stable_id.hpp>

namespace svanes {

std::optional<Entity> FindByStableId(const Registry &world,
                                     std::uint64_t value) {
    std::optional<Entity> found;
    world.ForEach<StableId>([&](Entity entity, const StableId &id) {
        if (id.value == value) {
            found = entity;
        }
    });
    return found;
}

} // namespace svanes
