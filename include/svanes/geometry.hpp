#pragma once

#include <svanes/circle_geometry.hpp>
#include <svanes/convex_polygon_geometry.hpp>
#include <svanes/rectangle_geometry.hpp>
#include <svanes/triangle_geometry.hpp>

#include <variant>
#include <vector>

namespace svanes {

/**
 * Represents the position and rotation of an entity's local origin in 2D space.
 * This component is used to determine where to place the entity's geometry in the world.
 * 
 * FIELDS:
 * - x: The x-coordinate of the entity's position.
 * - y: The y-coordinate of the entity's position.
 * - rotation: The rotation in radians.
 */
struct Transform {
    float x = 0.0F;
    float y = 0.0F;
    float rotation = 0.0F;
};

/**
 * Composes two transforms, applying the local transform relative to the parent transform.
 * The resulting transform represents the combined effect of both transforms.
 * 
 * @param parent The parent transform, representing the position and rotation of the parent entity.
 * @param local The local transform, representing the position and rotation of the child entity relative to the parent.
 * 
 * @return The composed transform, representing the position and rotation of the child entity in world coordinates.
 */
Transform ComposeTransforms(const Transform& parent, const Transform& local);

// The basic geometric primitives that can be immediately rendered or used for collision detection
// without needing to decompose them into simpler shapes. 
using Primitive2D = std::variant<Rectangle2D, Triangle2D, Circle2D, ConvexPolygon2D>;

/**
 * Represents a part of a composite shape, consisting of a geometric primitive and its associated transform.
 * This structure is used to define complex shapes that are composed of multiple simpler shapes.
 * 
 * FIELDS:
 * - shape: The geometric primitive that makes up this part of the composite shape.
 * - transform: The transform that specifies the position and rotation of this part 
 * relative to the composite shape's local origin.
 */
struct GeometryPart2D {
    Primitive2D shape;
    Transform transform;
};

/**
 * Represents a composite shape made up of multiple geometric parts, each with its own transform.
 * This structure allows for the creation of complex shapes that can be treated as a single entity for
 * rendering or collision detection purposes.
 * 
 * FIELDS:
 * - parts: A vector of GeometryPart2D objects that make up the composite shape.
 */
struct CompositeShape2D {
    std::vector<GeometryPart2D> parts;
};

// Represents any 2D geometric shape that can be used for rendering or collision detection.
using Geometry2D = std::variant<Rectangle2D, Triangle2D, Circle2D, ConvexPolygon2D, CompositeShape2D>;

}
