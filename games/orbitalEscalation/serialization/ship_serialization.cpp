#include "ship_serialization.hpp"

#include "json.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>

using nlohmann::json;

// the json library already reads arrays, floating point numbers, etc just fine.
// These functions are just for validating that the values are within the
// expected ranges and types.

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
static float ReadFloat(const json &value) {
    if (!value.is_number()) {
        throw std::invalid_argument("Ship value must be a number");
    }

    const double number = value.get<double>();
    if (!std::isfinite(number) ||
        std::abs(number) > std::numeric_limits<float>::max()) {
        throw std::invalid_argument("Ship number must fit a finite float");
    }

    return static_cast<float>(number);
}

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
static const json::array_t &ReadArray(const json &value, std::size_t size) {
    const auto &array = value.get_ref<const json::array_t &>();
    if (array.size() != size) {
        throw std::invalid_argument("Expected array of " +
                                    std::to_string(size) + " elements");
    }
    return array;
}

/**
 * Reads a JSON value as a signed 64-bit integer, ensuring it fits within the
 * range of int64_t.
 *
 * @param value The JSON value to read.
 *
 * @return The signed 64-bit integer value.
 *
 * @throws std::invalid_argument If the value is not an integer or does not fit
 * in int64_t.
 */
static std::int64_t ReadInteger(const json &value) {
    if (!value.is_number_integer() ||
        (value.is_number_unsigned() &&
         value.get<std::uint64_t>() >
             static_cast<std::uint64_t>(
                 std::numeric_limits<std::int64_t>::max()))) {
        throw std::invalid_argument(
            "Ship value must fit a signed 64-bit integer");
    }

    return value.get<std::int64_t>();
}

/**
 * Reads a JSON value as an unsigned 8-bit integer (color channel), ensuring it
 * is within the range of 0 to 255.
 *
 * @param value The JSON value to read.
 *
 * @return The unsigned 8-bit integer value representing the color channel.
 */
static std::uint8_t ReadColorChannel(const json &value) {
    const auto channel = ReadInteger(value);

    if (channel < 0 || channel > 255) {
        throw std::invalid_argument(
            "Ship color channels must be integers from 0 to 255");
    }

    return static_cast<std::uint8_t>(channel);
}

/**
 * Reads a JSON value as a 2D point (Vector2D), ensuring it is an array of
 * size 2.
 *
 * @param value The JSON value to read.
 *
 * @return The Vector2D representing the point.
 *
 * @throws std::invalid_argument If the value is not an array of size 2 or
 * contains invalid numbers.
 */
static svanes::Vector2D ReadPoint(const json &value) {
    const auto &point = ReadArray(value, 2);
    return {ReadFloat(point[0]), ReadFloat(point[1])};
}

/**
 * Writes a 2D point (Vector2D) to a JSON array.
 *
 * @param point The Vector2D to write.
 *
 * @return A JSON array representing the point.
 */
static json WritePoint(svanes::Vector2D point) {
    return json::array({point.x, point.y});
}

/**
 * Reads a JSON value as a geometric primitive (Rectangle2D, Circle2D,
 * Triangle2D, or ConvexPolygon2D).
 *
 * @param value The JSON value to read.
 *
 * @return A Primitive2D representing the geometric shape.
 *
 * @throws std::invalid_argument If the value does not represent a supported
 * primitive type.
 */
static svanes::Primitive2D ReadPrimitive(const json &value) {

    // Determine the type of the primitive shape from the "type" field in the
    // JSON object. .at() is used to access the value associated with the "type"
    // key, but this will be a reference to a JSON value. We then call
    // get_ref<const std::string &>() to get a reference to the underlying
    // string that the JSON value represents. As opposed to making a copy of the
    // string with .get

    const auto &type = value.at("type").get_ref<const std::string &>();

    if (type == "rectangle") {
        const auto center = ReadPoint(value.at("center"));
        const auto size = ReadPoint(value.at("size"));
        return svanes::Rectangle2D{center.x, center.y, size.x, size.y};
    }
    if (type == "circle") {
        const auto center = ReadPoint(value.at("center"));
        return svanes::Circle2D{center.x, center.y,
                                ReadFloat(value.at("radius"))};
    }

    // convex polygon
    if (type == "triangle") {
        const auto &vertices = ReadArray(value.at("vertices"), 3);
        return svanes::Triangle2D{{ReadPoint(vertices[0]),
                                   ReadPoint(vertices[1]),
                                   ReadPoint(vertices[2])}};
    }
    if (type == "polygon") {
        std::vector<svanes::Vector2D> vertices;
        for (const auto &vertex :
             value.at("vertices").get_ref<const json::array_t &>()) {
            vertices.push_back(ReadPoint(vertex));
        }
        return svanes::ConvexPolygon2D(std::move(vertices));
    }
    throw std::invalid_argument("Unsupported primitive type: " + type);
}

/**
 * Reads a JSON value as a geometric shape (Primitive2D or CompositeShape2D).
 *
 * @param value The JSON value to read.
 *
 * @return A Geometry2D representing the geometric shape.
 *
 * @throws std::invalid_argument If the value does not represent a supported
 * shape type.
 */
static svanes::Geometry2D ReadGeometry(const json &value) {
    svanes::Geometry2D geometry;

    // Composites cannot be nested, so we don't need to recurse into
    // ReadGeometry luckily.
    if (value.at("type").get_ref<const std::string &>() == "composite") {
        svanes::CompositeShape2D composite;

        // For each primitive part of the overall composite shape...
        for (const auto &part :
             value.at("parts").get_ref<const json::array_t &>()) {

            //... we get the transform for that part, if it exists. If it
            //doesn't exist, we use the default transform (0, 0, 0)
            // since vertex positions can already specify the location of the
            // part relative to the composite shape's origin.

            svanes::Transform transform;
            if (const auto found = part.find("transform");
                found != part.end()) {
                const auto &pose = ReadArray(*found, 3);
                transform = {ReadFloat(pose[0]), ReadFloat(pose[1]),
                             ReadFloat(pose[2])};
            }

            composite.parts.push_back(
                {ReadPrimitive(part.at("shape")), transform});
        }

        geometry = std::move(composite);
    } else {
        geometry = std::visit(
            [](auto primitive) -> svanes::Geometry2D { return primitive; },
            ReadPrimitive(value));
    }

    // Will run various geometry validation checks.
    svanes::ComputeBounds(geometry, {});
    return geometry;
}

/**
 * Writes a rectangle shape to JSON format.
 *
 * @param shape The Rectangle2D shape to write.
 *
 * @return A JSON object representing the rectangle shape.
 */
static json WriteShape(const svanes::Rectangle2D &shape) {
    return {{"type", "rectangle"},
            {"center", WritePoint({shape.x, shape.y})},
            {"size", WritePoint({shape.width, shape.height})}};
}

/**
 * Writes a circle shape to JSON format.
 *
 * @param shape The Circle2D shape to write.
 *
 * @return A JSON object representing the circle shape.
 */
static json WriteShape(const svanes::Circle2D &shape) {
    return {{"type", "circle"},
            {"center", WritePoint({shape.x, shape.y})},
            {"radius", shape.radius}};
}

/**
 * Writes a polygon shape to JSON format.
 *
 * @tparam Vertices The type of the vertices container (e.g.,
 * std::vector<Vector2D>).
 *
 * @param vertices The container of vertices representing the polygon.
 * @param type The type of the polygon (e.g., "triangle", "polygon").
 *
 * @return A JSON object representing the convex polygon shape.
 */
template <typename Vertices>
static json WriteVertices(const Vertices &vertices, const std::string &type) {
    auto points = json::array();
    for (const auto vertex : vertices) {
        points.push_back(WritePoint(vertex));
    }
    return {{"type", type}, {"vertices", std::move(points)}};
}

/**
 * Writes a triangle shape to JSON format.
 *
 * @param shape The Triangle2D shape to write.
 *
 * @return A JSON object representing the triangle shape.
 */
static json WriteShape(const svanes::Triangle2D &shape) {
    return WriteVertices(shape.vertices, "triangle");
}

/**
 * Writes a convex polygon shape to JSON format.
 *
 * @param shape The ConvexPolygon2D shape to write.
 *
 * @return A JSON object representing the convex polygon shape.
 */
static json WriteShape(const svanes::ConvexPolygon2D &shape) {
    return WriteVertices(shape.Vertices(), "polygon");
}

/**
 * Writes a composite shape to JSON format.
 *
 * @param shape The CompositeShape2D shape to write.
 *
 * @return A JSON object representing the composite shape.
 */
static json WriteShape(const svanes::CompositeShape2D &shape) {
    auto parts = json::array();
    for (const auto &part : shape.parts) {

        // visit every part of the composite shape and write the primitive shape
        // and transform to JSON format.
        parts.push_back(
            {{"shape",
              std::visit(
                  [](const auto &primitive) { return WriteShape(primitive); },
                  part.shape)},
             {"transform",
              {part.transform.x, part.transform.y, part.transform.rotation}}});
    }

    return {{"type", "composite"}, {"parts", std::move(parts)}};
}

/**
 * Writes a geometric shape (Primitive2D or CompositeShape2D) to JSON format.
 *
 * @param geometry The Geometry2D shape to write.
 *
 * @return A JSON object representing the geometric shape.
 */
static json WriteGeometry(const svanes::Geometry2D &geometry) {
    svanes::ComputeBounds(geometry, {});
    return std::visit([](const auto &shape) { return WriteShape(shape); },
                      geometry);
}

/**
 * Appends the parts of a composite collider geometry to another composite
 * collider geometry.
 *
 * @param collider The composite collider geometry to which parts will be
 * appended.
 * @param shape The composite shape whose parts will be appended to the
 * collider.
 */
static void AppendCollision(svanes::CompositeShape2D &collider,
                            const svanes::CompositeShape2D &shape) {
    collider.parts.insert(collider.parts.end(), shape.parts.begin(),
                          shape.parts.end());
}

/**
 * Appends a primitive shape to a composite collider geometry.
 *
 * @tparam Primitive The type of the primitive shape (e.g., Rectangle2D,
 * Circle2D, etc.).
 *
 * @param collider The composite collider geometry to which the primitive will
 * be appended.
 * @param shape The primitive shape to append to the collider.
 */
template <typename Primitive>
static void AppendCollision(svanes::CompositeShape2D &collider,
                            const Primitive &shape) {

    // empty transform is used here because the primitive shape is already
    // defined in its own local coordinate space
    collider.parts.push_back({shape, {}});
}

/**
 * Validates the acceleration limits of a ship definition, ensuring they are
 * finite and nonnegative.
 *
 * @param definition The ShipDefinition to validate.
 *
 * @throws std::invalid_argument If any of the acceleration limits are not
 * finite or are negative.
 */
static void ValidateLimits(const ShipDefinition &definition) {
    if (!std::isfinite(definition.max_acceleration) ||
        definition.max_acceleration < 0 ||
        !std::isfinite(definition.max_angular_acceleration) ||
        definition.max_angular_acceleration < 0) {
        throw std::invalid_argument(
            "Ship acceleration limits must be finite and nonnegative");
    }
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
    };

    ValidateLimits(definition);

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
        svanes::SolidShape solid{
            {ReadColorChannel(rgba[0]), ReadColorChannel(rgba[1]),
             ReadColorChannel(rgba[2]), ReadColorChannel(rgba[3])},
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
        const auto z_order = ReadInteger(visual.at("z_order"));
        if (z_order < std::numeric_limits<std::int32_t>::min() ||
            z_order > std::numeric_limits<std::int32_t>::max()) {
            throw std::invalid_argument("Ship z_order must fit an int32_t");
        }

        // Append the visual component's geometry to the overall visual geometry
        // of the ship.
        std::visit(
            [&](const auto &shape) {
                AppendCollision(visual_collision, shape);
            },
            solid.geometry);

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
    ValidateLimits(definition);

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

    // We do not bother checking if the collider is equal to the visuals.
    // So even if we could serialize the collider as "visuals", we always
    // serialize it as a geometry object. In the future, this could be
    // optimized...
    return {{"max_acceleration", definition.max_acceleration},
            {"max_angular_acceleration", definition.max_angular_acceleration},
            {"collider", WriteGeometry(definition.collider.geometry)},
            {"visuals", std::move(visuals)}};
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
