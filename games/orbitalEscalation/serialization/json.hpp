#pragma once

#include <nlohmann/json.hpp>

#include <filesystem>

/**
 * Reads a JSON file from the given path and returns the parsed JSON value.
 *
 * @param path The path to the JSON file.
 *
 * @return The parsed JSON value.
 *
 * @throws std::runtime_error If the file cannot be opened or read, or if the
 * JSON is invalid.
 */
nlohmann::json ReadJson(const std::filesystem::path &path);

/**
 * Writes a JSON value to a file at the given path.
 *
 * @param path The path to the JSON file.
 * @param value The JSON value to write.
 *
 * @throws std::runtime_error If the file cannot be opened or written to.
 */
void WriteJson(const std::filesystem::path &path, const nlohmann::json &value);
