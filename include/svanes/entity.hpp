#pragma once

#include <cstdint>

namespace svanes {

struct Entity {
    std::uint32_t index = 0;
    std::uint32_t generation = 0;

    friend bool operator==(const Entity&, const Entity&) = default;
};

inline constexpr Entity kInvalidEntity{};

} // namespace svanes
