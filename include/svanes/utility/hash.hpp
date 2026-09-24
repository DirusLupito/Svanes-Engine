#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace svanes {

/**
 * Computes a hash value for a given span of 32-bit unsigned integers.
 * Useful for hashing vectors of integers to use as keys in unordered
 * containers. 
 *
 * @param values A span of 32-bit unsigned integers to be hashed.
 *
 * @return A size_t hash of the input values.
 */
std::size_t Hash(std::span<const std::uint32_t> values) noexcept;

} // namespace svanes
