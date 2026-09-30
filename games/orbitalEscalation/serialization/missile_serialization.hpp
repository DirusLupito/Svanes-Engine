#pragma once

#include "../game_objects/dynamic_object/missile.hpp"

#include <filesystem>
#include <string>
#include <string_view>

/**
 * Deserializes a missile definition from a JSON string.
 *
 * @param text The JSON string representing the missile definition.
 *
 * @return The deserialized MissileDefinition object.
 *
 * @throws std::invalid_argument If the JSON is invalid or does not conform to
 * the expected missile definition format.
 */
MissileDefinition DeserializeMissile(std::string_view text);

/**
 * Serializes a missile definition to a JSON string.
 *
 * @param definition The MissileDefinition object to serialize.
 *
 * @return A JSON string representing the missile definition.
 *
 * @throws std::invalid_argument If the missile definition contains unsupported
 * or invalid data.
 */
std::string SerializeMissile(const MissileDefinition &definition);

/**
 * Loads a missile definition from a JSON file.
 *
 * @param path The path to the JSON file containing the missile definition.
 *
 * @return The loaded MissileDefinition object.
 *
 * @throws std::invalid_argument If the file cannot be read or the JSON is
 * invalid.
 */
MissileDefinition LoadMissile(const std::filesystem::path &path);

/**
 * Saves a missile definition to a JSON file.
 *
 * @param path The path to the JSON file where the missile definition will be
 * saved.
 *
 * @param definition The MissileDefinition object to save.
 *
 * @throws std::invalid_argument If the file cannot be written or the JSON is
 * invalid.
 */
void SaveMissile(const std::filesystem::path &path,
                 const MissileDefinition &definition);
