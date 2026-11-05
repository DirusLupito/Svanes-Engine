#pragma once

#include <cstddef>
#include <filesystem>
#include <span>
#include <vector>

namespace svanes {

/**
 * Reads the complete contents of a binary file.
 *
 * @param path The file to read.
 *
 * @return The bytes read from the file, empty if the file is empty.
 *
 * @throws std::runtime_error if the file cannot be opened or read.
 * @throws std::length_error if the file size is invalid or too large to read.
 */
std::vector<std::byte> ReadFile(const std::filesystem::path &path);

/**
 * Writes bytes to a binary file, replacing its previous contents. Creates the
 * file if it does not exist.
 *
 * @param path The destination file. Its parent directory must exist.
 *
 * @param bytes The bytes to write. An empty span creates or empties the file.
 *
 * @throws std::runtime_error if the file cannot be opened, written, or closed.
 * @throws std::length_error if bytes is too large to write.
 */
void WriteFile(const std::filesystem::path &path,
               std::span<const std::byte> bytes);

} // namespace svanes
