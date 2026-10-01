#include <svanes/utility/hash.hpp>

namespace svanes {

std::size_t Hash(std::span<const std::uint32_t> values) noexcept {
    // Adapted from see's answer at:
    // https://stackoverflow.com/questions/20511347/a-good-hash-function-for-a-vector
    std::size_t seed = values.size();
    for (std::uint32_t x : values) {
        x = ((x >> 16) ^ x) * 0x45d9f3bU;
        x = ((x >> 16) ^ x) * 0x45d9f3bU;
        x = (x >> 16) ^ x;
        seed ^= x + 0x9e3779b9U + (seed << 6) + (seed >> 2);
    }
    return seed;
}

std::uint64_t HashBytes(std::span<const std::byte> bytes) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const std::byte byte : bytes) {
        hash ^= std::to_integer<std::uint8_t>(byte);
        hash *= 1099511628211ULL;
    }
    return hash;
}

} // namespace svanes
