#pragma once

#include "../game_objects/dynamic_object/dynamic_object.hpp"
#include "json.hpp"

/**
 * Reads the type of a dynamic object from a JSON string.
 *
 * @param value The JSON string, "ship", "missile", or "missile_launcher".
 * @return The corresponding DynamicObjectType.
 * @throws std::invalid_argument If the type is unsupported.
 */
DynamicObjectType ReadDynamicObjectType(const nlohmann::json &value);

/**
 * Writes the type of a dynamic object as a string for use in JSON.
 *
 * @param type The DynamicObjectType to write.
 * @return The string "ship", "missile", or "missile_launcher".
 * @throws std::invalid_argument If the type is unsupported.
 */
std::string WriteDynamicObjectType(DynamicObjectType type);

/**
 * Reads the properties shared by dynamic object definitions. Attachment
 * references are read here, but the catalog checks whether they exist after
 * all files have been loaded. Otherwise, the order of the files would matter.
 *
 * @param root The JSON object representing the dynamic object definition.
 * @return A DynamicObjectDefinition constructed from the JSON data.
 * @throws std::invalid_argument If any properties are invalid or unsupported.
 */
DynamicObjectDefinition ReadDynamicObject(const nlohmann::json &root);

/**
 * Writes the properties shared by dynamic object definitions. Attachments
 * remain references to other definitions, rather than copies of those
 * definitions.
 *
 * @param definition The DynamicObjectDefinition to write.
 * @return A JSON object representing the dynamic object definition.
 * @throws std::invalid_argument If any properties are invalid or unsupported.
 */
nlohmann::json WriteDynamicObject(const DynamicObjectDefinition &definition);
