#include "geometry_serialization.hpp"

#include <stdexcept>
#include <utility>

using nlohmann::json;

svanes::Vector2D ReadPoint(const json &value) {
    const auto &point = ReadArray(value, 2);
    return {ReadFloat(point[0]), ReadFloat(point[1])};
}

json WritePoint(svanes::Vector2D point) {
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

svanes::Geometry2D ReadGeometry(const json &value) {
    svanes::Geometry2D geometry;

    // Composites cannot be nested, so we don't need to recurse into
    // ReadGeometry luckily.
    if (value.at("type").get_ref<const std::string &>() == "composite") {
        svanes::CompositeShape2D composite;

        // For each primitive part of the overall composite shape...
        for (const auto &part :
             value.at("parts").get_ref<const json::array_t &>()) {

            //... we get the transform for that part, if it exists. If it
            // doesn't exist, we use the default transform (0, 0, 0)
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

json WriteGeometry(const svanes::Geometry2D &geometry) {
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

void AppendCollision(svanes::CompositeShape2D &collider,
                     const svanes::Geometry2D &geometry) {
    std::visit([&](const auto &shape) { AppendCollision(collider, shape); },
               geometry);
}
