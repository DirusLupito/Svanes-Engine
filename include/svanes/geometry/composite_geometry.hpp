#pragma once

#include <svanes/geometry/primitive_geometry.hpp>

#include <optional>
#include <vector>

namespace svanes {

/**
 * Represents a part of a composite shape, consisting of a geometric primitive
 * and its associated transform. This structure is used to define complex shapes
 * that are composed of multiple simpler shapes.
 *
 * FIELDS:
 * - shape: The geometric primitive that makes up this part of the composite
 * shape.
 * - transform: The transform that specifies the position and rotation of this
 * part relative to the composite shape's local origin.
 */
struct GeometryPart2D {
    Primitive2D shape;
    Transform transform;
};

/**
 * Represents a composite shape made up of multiple geometric parts, each with
 * its own transform. This structure allows for the creation of complex shapes
 * that can be treated as a single entity for rendering or collision detection
 * purposes.
 *
 * FIELDS:
 * - parts: A vector of GeometryPart2D objects that make up the composite shape.
 */
struct CompositeShape2D {
    std::vector<GeometryPart2D> parts;
};

/**
 * Represents a composite shape in 2D space, which is composed of multiple
 * geometric primitives, each with its own transform. This class allows for the
 * creation of complex shapes by combining simpler shapes.
 */
class CompositeGeometry final {
public:
    /**
     * Constructs a CompositeGeometry object from a CompositeShape2D.
     *
     * @param composite The composite shape to be represented by this geometry.
     */
    explicit CompositeGeometry(CompositeShape2D composite);

    /**
     * Calculates the axis-aligned bounding box of the composite shape after
     * applying a given transform. The transform is applied to each part of the
     * composite shape, and the resulting bounding boxes are combined to form
     * the overall bounding box.
     *
     * @param transform The transform to apply to the composite shape.
     *
     * @return An optional Rectangle2D representing the axis-aligned bounding
     * box of the transformed composite shape. If the composite shape has no
     * parts, std::nullopt is returned.
     *
     * @throws std::invalid_argument if the provided transform contains
     * non-finite values for its position or rotation.
     */
    std::optional<Rectangle2D> Bounds(const Transform &transform) const;

private:
    // The underlying struct holding the composite shape's parts and their
    // transforms.
    CompositeShape2D composite;
};

} // namespace svanes
