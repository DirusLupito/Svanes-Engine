#pragma once

#include <nlohmann/json.hpp>

#include <cstddef>
#include <cstdint>
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

/**
 * Reads a JSON value as a float, ensuring it is finite and within the range of
 * a float.
 *
 * @param value The JSON value to read.
 *
 * @return The float value.
 *
 * @throws std::invalid_argument If the value is not a number or does not fit in
 * a float.
 */
float ReadFloat(const nlohmann::json &value);


/**
 * Reads a JSON value as an array of a specific size, ensuring it matches the
 * expected size.
 *
 * @param value The JSON value to read.
 * @param size The expected size of the array.
 *
 * @return A reference to the JSON array.
 *
 * @throws std::invalid_argument If the value is not an array or does not match
 * the expected size.
 */
const nlohmann::json::array_t &ReadArray(const nlohmann::json &value,
                                         std::size_t size);


/**
 * Reads a JSON value as a signed 64-bit integer, ensuring it fits within the
 * range of std::int64_t.
 *
 * @param value The JSON value to read.
 *
 * @return The signed 64-bit integer value.
 *
 * @throws std::invalid_argument If the value is not an integer or does not fit
 * in std::int64_t.
 */
std::int64_t ReadInt64(const nlohmann::json &value);


/**
 * Reads a JSON value as an unsigned 8-bit integer, ensuring it fits within the
 * range of std::uint8_t (0 to 255).
 *
 * @param value The JSON value to read.
 *
 * @return The unsigned 8-bit integer value.
 *
 * @throws std::invalid_argument If the value is not an integer or does not fit
 * in std::uint8_t.
 */
std::uint8_t ReadUInt8(const nlohmann::json &value);
