#include "ship_serialization.hpp"

#include "../propulsion_system.hpp"
#include "dynamic_object_serialization.hpp"
#include "geometry_serialization.hpp"
#include "json.hpp"

#include <stdexcept>

using nlohmann::json;

/**
 * Validates the propulsion of a ship definition, ensuring its acceleration
 * limits are finite and nonnegative, and its forward direction is valid.
 *
 * @param definition The ShipDefinition to validate.
 *
 * @throws std::invalid_argument If any of the acceleration limits are not
 * finite or are negative, or the forward direction has a nonfinite or zero
 * length.
 */
static void ValidatePropulsion(const ShipDefinition &definition) {
    Propulsion{definition.max_acceleration, definition.max_angular_acceleration,
               definition.forward};
}

/**
 * Reads a ship definition from a JSON object, validating its properties and
 * constructing a ShipDefinition.
 *
 * @param root The JSON object representing the ship definition.
 *
 * @return A ShipDefinition constructed from the JSON data.
 *
 * @throws std::invalid_argument If any of the ship properties are invalid or
 * unsupported.
 */
static ShipDefinition ReadShip(const json &root) {
    ShipDefinition definition{
        .max_acceleration = ReadFloat(root.at("max_acceleration")),
        .max_angular_acceleration =
            ReadFloat(root.at("max_angular_acceleration")),
        .forward = ReadPoint(root.at("forward")),
    };

    ValidatePropulsion(definition);

    definition.object = ReadDynamicObject(root);
    return definition;
}

/**
 * Writes a ship definition to a JSON object, including its properties and
 * visual components.
 *
 * @param definition The ShipDefinition to write.
 *
 * @return A JSON object representing the ship definition.
 *
 * @throws std::invalid_argument If any of the ship properties are invalid or
 * unsupported.
 */
static json WriteShip(const ShipDefinition &definition) {
    ValidatePropulsion(definition);

    auto result = WriteDynamicObject(definition.object);
    result["max_acceleration"] = definition.max_acceleration;
    result["max_angular_acceleration"] = definition.max_angular_acceleration;
    result["forward"] = WritePoint(definition.forward);
    return result;
}

ShipDefinition DeserializeShip(std::string_view text) {
    return ReadShip(json::parse(text));
}
std::string SerializeShip(const ShipDefinition &definition) {
    return WriteShip(definition).dump(2) + '\n';
}

ShipDefinition LoadShip(const std::filesystem::path &path) {
    const auto json = ReadJson(path);
    try {
        return ReadShip(json);
    } catch (const std::exception &error) {
        throw std::invalid_argument(path.string() + ": " + error.what());
    }
}

void SaveShip(const std::filesystem::path &path,
              const ShipDefinition &definition) {
    WriteJson(path, WriteShip(definition));
}
