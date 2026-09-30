#pragma once

#include "../game_objects/dynamic_object/missile_launcher.hpp"

#include <filesystem>
#include <string>
#include <string_view>

/**
 * Deserializes a missile launcher definition from a JSON string.
 *
 * @param text The JSON string representing the missile launcher definition.
 *
 * @return The deserialized MissileLauncherDefinition object.
 *
 * @throws std::invalid_argument If the JSON is invalid or does not conform to
 * the expected missile launcher definition format.
 */
MissileLauncherDefinition DeserializeMissileLauncher(std::string_view text);

/**
 * Serializes a missile launcher definition to a JSON string.
 *
 * @param definition The MissileLauncherDefinition object to serialize.
 *
 * @return A JSON string representing the missile launcher definition.
 *
 * @throws std::invalid_argument If the missile launcher definition contains
 * unsupported or invalid data.
 */
std::string
SerializeMissileLauncher(const MissileLauncherDefinition &definition);

/**
 * Loads a missile launcher definition from a JSON file.
 *
 * @param path The path to the JSON file containing the missile launcher
 * definition.
 *
 * @return The loaded MissileLauncherDefinition object.
 *
 * @throws std::invalid_argument If the file cannot be read or the JSON is
 * invalid.
 */
MissileLauncherDefinition
LoadMissileLauncher(const std::filesystem::path &path);

/**
 * Saves a missile launcher definition to a JSON file.
 *
 * @param path The path to the JSON file where the missile launcher definition
 * will be saved.
 *
 * @param definition The MissileLauncherDefinition object to save.
 *
 * @throws std::invalid_argument If the file cannot be written or the JSON is
 * invalid.
 */
void SaveMissileLauncher(const std::filesystem::path &path,
                         const MissileLauncherDefinition &definition);
