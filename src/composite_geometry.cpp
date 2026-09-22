#include <svanes/composite_geometry.hpp>
#include <svanes/geometry.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace svanes {

CompositeGeometry::CompositeGeometry(CompositeShape2D composite)
    : composite(std::move(composite)) {}

std::optional<Rectangle2D>
CompositeGeometry::Bounds(const Transform &transform) const {
    if (!std::isfinite(transform.x) || !std::isfinite(transform.y) ||
        !std::isfinite(transform.rotation)) {
        throw std::invalid_argument(
            "Composite bounds require a finite transform.");
    }

    if (composite.parts.empty()) {
        return std::nullopt;
    }

    // Basically we iterate over all the vertices of all the parts.
    // But we use the abstraction that is provided by each part's Bounds()
    // since that means if we ever want to do something really wacky
    // we need only update the Bounds() function for that part's type.

    double min_x = std::numeric_limits<double>::infinity();
    double min_y = min_x;
    double max_x = -min_x;
    double max_y = max_x;

    // benefit of not letting composites be nested is that we don't have to
    // recurse here.

    for (const GeometryPart2D &part : composite.parts) {
        const Transform pose = ComposeTransforms(transform, part.transform);
        const Rectangle2D bounds =
            internal::ComputePrimitiveBounds(part.shape, pose);
        min_x =
            std::min(min_x, static_cast<double>(bounds.x) - bounds.width * 0.5);
        min_y = std::min(min_y,
                         static_cast<double>(bounds.y) - bounds.height * 0.5);
        max_x =
            std::max(max_x, static_cast<double>(bounds.x) + bounds.width * 0.5);
        max_y = std::max(max_y,
                         static_cast<double>(bounds.y) + bounds.height * 0.5);
    }

    return internal::BoundsFromExtents(min_x, min_y, max_x, max_y);
}

} // namespace svanes
