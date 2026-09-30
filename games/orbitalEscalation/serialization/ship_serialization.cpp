#include "ship_serialization.hpp"

#include "../propulsion_system.hpp"
#include "geometry_serialization.hpp"
#include "json.hpp"

#include <cmath>
#include <limits>
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

    // Represents the visual geometry of the ship, which can be used as the
    // collider if the "collider" field in the JSON is set to "visuals".
    svanes::CompositeShape2D visual_collision;
    for (const auto &visual :
         root.at("visuals").get_ref<const json::array_t &>()) {

        // The color of this particular visual component of the ship,
        // represented as an RGBA array.
        const auto &rgba = ReadArray(visual.at("color"), 4);

        // TODO: Support more visual component types than just solid shapes.

        // The shape of this particular visual component of the ship.
        svanes::SolidShape solid{{ReadUInt8(rgba[0]), ReadUInt8(rgba[1]),
                                  ReadUInt8(rgba[2]), ReadUInt8(rgba[3])},
                                 ReadGeometry(visual.at("geometry"))};

        // Determine how to do alpha blending for this visual component.
        if (const auto found = visual.find("blend_mode");
            found != visual.end()) {
            const auto &blend = found->get_ref<const std::string &>();

            if (blend != "alpha" && blend != "additive") {
                throw std::invalid_argument("Unsupported ship blend mode: " +
                                            blend);
            }

            solid.blend_mode = blend == "alpha" ? svanes::BlendMode::Alpha
                                                : svanes::BlendMode::Additive;
        }

        // The z-order of this visual component. Lower z-order values are drawn
        // first (below) and higher z-order values are drawn later (on top).
        const auto z_order = ReadInt64(visual.at("z_order"));
        if (z_order < std::numeric_limits<std::int32_t>::min() ||
            z_order > std::numeric_limits<std::int32_t>::max()) {
            throw std::invalid_argument("Ship z_order must fit an int32_t");
        }

        // Append the visual component's geometry to the overall visual geometry
        // of the ship.
        AppendCollision(visual_collision, solid.geometry);

        definition.visuals.push_back(
            {std::move(solid), static_cast<std::int32_t>(z_order)});
    }
    const auto &collider = root.at("collider");

    // if the collider isn't a json dictionary, and instead a string, it should
    // be the string "visuals", which means that the ship's collider is defined
    // by its visual geometry. Otherwise, we read the collider geometry from the
    // json dictionary.
    if (collider.is_string()) {
        if (collider.get_ref<const std::string &>() != "visuals") {
            throw std::invalid_argument(
                "Ship collider string must be 'visuals'");
        }

        definition.collider.geometry = std::move(visual_collision);
    } else {
        definition.collider.geometry = ReadGeometry(collider);
    }

    // Read the attachments of the ship, if any, from the JSON object. Each
    // attachment consists of a transform and a nested definition.
    // TODO: Allow for more than just ships to be attached.

    if (const auto found = root.find("attachments"); found != root.end()) {
        for (const auto &entry : found->get_ref<const json::array_t &>()) {
            const auto &pose = ReadArray(entry.at("transform"), 3);
            definition.attachments.push_back(
                {{ReadFloat(pose[0]), ReadFloat(pose[1]), ReadFloat(pose[2])},
                 ReadShip(entry.at("ship"))});
        }
    }
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

    json::array_t visuals;
    for (const auto &visual : definition.visuals) {

        // TODO: Support more visual component types than just solid shapes.

        const auto *solid = std::get_if<svanes::SolidShape>(&visual.component);
        if (!solid) {
            throw std::invalid_argument(
                "Ship serialization currently supports only solid visuals");
        }

        if (solid->blend_mode != svanes::BlendMode::Alpha &&
            solid->blend_mode != svanes::BlendMode::Additive) {
            throw std::invalid_argument("Unsupported ship blend mode");
        }

        const auto color = solid->color;
        visuals.push_back(
            {{"geometry", WriteGeometry(solid->geometry)},
             {"color", {color.red, color.green, color.blue, color.alpha}},
             {"z_order", visual.z_order},
             {"blend_mode", solid->blend_mode == svanes::BlendMode::Alpha
                                ? "alpha"
                                : "additive"}});
    }

    json::array_t attachments;
    for (const auto &attachment : definition.attachments) {
        const auto &pose = attachment.transform;
        if (!std::isfinite(pose.x) || !std::isfinite(pose.y) ||
            !std::isfinite(pose.rotation)) {
            throw std::invalid_argument("Attachment transform must be finite");
        }

        attachments.push_back({{"transform", {pose.x, pose.y, pose.rotation}},
                               {"ship", WriteShip(attachment.ship)}});
    }

    // We do not bother checking if the collider is equal to the visuals.
    // So even if we could serialize the collider as "visuals", we always
    // serialize it as a geometry object. In the future, this could be
    // optimized...
    return {{"max_acceleration", definition.max_acceleration},
            {"max_angular_acceleration", definition.max_angular_acceleration},
            {"forward", WritePoint(definition.forward)},
            {"collider", WriteGeometry(definition.collider.geometry)},
            {"visuals", std::move(visuals)},
            {"attachments", std::move(attachments)}};
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
