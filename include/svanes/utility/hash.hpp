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

/**
 * Hashes bytes with 64-bit FNV-1a
 * (https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function).
 * The result depends only on the bytes, so peers can compare hashes of values
 * they encoded the same way.
 * @param bytes The bytes to hash.
 * @return The hash.
 */
std::uint64_t HashBytes(std::span<const std::byte> bytes);

} // namespace svanes
