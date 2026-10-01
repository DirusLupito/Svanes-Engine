#pragma once

#include "../game_objects/dynamic_object/ship.hpp"

#include <filesystem>
#include <string>
#include <string_view>

/**
 * Deserializes a ship definition from a JSON string.
 *
 * @param text The JSON string representing the ship definition.
 *
 * @return The deserialized ShipDefinition object.
 *
 * @throws std::invalid_argument If the JSON is invalid or does not conform to
 * the expected ship definition format.
 */
ShipDefinition DeserializeShip(std::string_view text);

/**
 * Serializes a ship definition to a JSON string.
 *
 * @param definition The ShipDefinition object to serialize.
 *
 * @return A JSON string representing the ship definition.
 *
 * @throws std::invalid_argument If the ship definition contains unsupported or
 * invalid data.
 */
std::string SerializeShip(const ShipDefinition &definition);

/**
 * Loads a ship definition from a JSON file.
 *
 * @param path The path to the JSON file containing the ship definition.
 *
 * @return The loaded ShipDefinition object.
 *
 * @throws std::invalid_argument If the file cannot be read or the JSON is
 * invalid.
 */
ShipDefinition LoadShip(const std::filesystem::path &path);

/**
 * Saves a ship definition to a JSON file.
 *
 * @param path The path to the JSON file where the ship definition will be
 * saved.
 *
 * @param definition The ShipDefinition object to save.
 *
 * @throws std::invalid_argument If the file cannot be written or the JSON is
 * invalid.
 */
void SaveShip(const std::filesystem::path &path,
              const ShipDefinition &definition);
