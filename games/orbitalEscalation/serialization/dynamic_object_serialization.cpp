#include "dynamic_object_serialization.hpp"

#include "geometry_serialization.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>

using nlohmann::json;

DynamicObjectType ReadDynamicObjectType(const json &value) {
    const auto &type = value.get_ref<const std::string &>();
    if (type == "ship") {
        return DynamicObjectType::Ship;
    }

    if (type == "missile") {
        return DynamicObjectType::Missile;
    }

    throw std::invalid_argument("Unsupported dynamic object type: " + type);
}

std::string WriteDynamicObjectType(DynamicObjectType type) {
    switch (type) {
    case DynamicObjectType::Ship:
        return "ship";
    case DynamicObjectType::Missile:
        return "missile";
    }

    throw std::invalid_argument("Unsupported dynamic object type");
}

DynamicObjectDefinition ReadDynamicObject(const json &root) {
    DynamicObjectDefinition definition;
    definition.name = root.at("name").get<std::string>();
    if (definition.name.empty()) {
        throw std::invalid_argument("Dynamic object name must not be empty");
    }

    // Represents the visual geometry of the object, which can be used as the
    // collider if the "collider" field in the JSON is set to "visuals".
    svanes::CompositeShape2D visual_collision;
    for (const auto &visual :
         root.at("visuals").get_ref<const json::array_t &>()) {

        // The color of this particular visual component of the object,
        // represented as an RGBA array.
        const auto &rgba = ReadArray(visual.at("color"), 4);

        // TODO: Support more visual component types than just solid shapes.

        // The shape of this particular visual component of the object.
        svanes::SolidShape solid{{ReadUInt8(rgba[0]), ReadUInt8(rgba[1]),
                                  ReadUInt8(rgba[2]), ReadUInt8(rgba[3])},
                                 ReadGeometry(visual.at("geometry"))};

        // Determine how to do alpha blending for this visual component.
        if (const auto found = visual.find("blend_mode");
            found != visual.end()) {
            const auto &blend = found->get_ref<const std::string &>();

            if (blend != "alpha" && blend != "additive") {
                throw std::invalid_argument("Unsupported object blend mode: " +
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
            throw std::invalid_argument("Object z_order must fit an int32_t");
        }

        // Append the visual component's geometry to the overall visual geometry
        // of the object.
        AppendCollision(visual_collision, solid.geometry);

        definition.visuals.push_back(
            {std::move(solid), static_cast<std::int32_t>(z_order)});
    }
    const auto &collider = root.at("collider");

    // if the collider isn't a json dictionary, and instead a string, it should
    // be the string "visuals", which means that the object's collider is
    // defined by its visual geometry. Otherwise, we read the collider geometry
    // from the json dictionary.
    if (collider.is_string()) {
        if (collider.get_ref<const std::string &>() != "visuals") {
            throw std::invalid_argument(
                "Object collider string must be 'visuals'");
        }

        definition.collider.geometry = std::move(visual_collision);
    } else {
        definition.collider.geometry = ReadGeometry(collider);
    }

    // Read the attachments of the object, if any, from the JSON object. Each
    // attachment consists of a transform and a reference to another definition.
    // We cannot check whether that definition exists yet, as its file might not
    // have been read. The catalog checks these references after loading all
    // files.
    if (const auto found = root.find("attachments"); found != root.end()) {
        for (const auto &entry : found->get_ref<const json::array_t &>()) {
            const auto &pose = ReadArray(entry.at("transform"), 3);
            const auto &name = entry.at("name").get_ref<const std::string &>();
            if (name.empty()) {
                throw std::invalid_argument(
                    "Attachment name must not be empty");
            }
            definition.attachments.push_back(
                {ReadDynamicObjectType(entry.at("type")),
                 name,
                 {ReadFloat(pose[0]), ReadFloat(pose[1]), ReadFloat(pose[2])}});
        }
    }
    return definition;
}

json WriteDynamicObject(const DynamicObjectDefinition &definition) {
    if (definition.name.empty()) {
        throw std::invalid_argument("Dynamic object name must not be empty");
    }
    json::array_t visuals;
    for (const auto &visual : definition.visuals) {

        // TODO: Support more visual component types than just solid shapes.

        const auto *solid = std::get_if<svanes::SolidShape>(&visual.component);
        if (!solid) {
            throw std::invalid_argument(
                "Object serialization currently supports only solid visuals");
        }

        if (solid->blend_mode != svanes::BlendMode::Alpha &&
            solid->blend_mode != svanes::BlendMode::Additive) {
            throw std::invalid_argument("Unsupported object blend mode");
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

        if (attachment.name.empty()) {
            throw std::invalid_argument("Attachment name must not be empty");
        }
        attachments.push_back(
            {{"transform", {pose.x, pose.y, pose.rotation}},
             {"type", WriteDynamicObjectType(attachment.type)},
             {"name", attachment.name}});
    }

    // We do not bother checking if the collider is equal to the visuals.
    // So even if we could serialize the collider as "visuals", we always
    // serialize it as a geometry object. In the future, this could be
    // optimized...
    return {{"name", definition.name},
            {"collider", WriteGeometry(definition.collider.geometry)},
            {"visuals", std::move(visuals)},
            {"attachments", std::move(attachments)}};
}
