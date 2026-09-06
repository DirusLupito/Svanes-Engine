#include <svanes/collision_system.hpp>

#include <svanes/rectangle_geometry.hpp>
#include <svanes/triangle_geometry.hpp>
#include <svanes/render/render_system.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <stdexcept>
#include <utility>

namespace svanes {

/**
 * Validates that a rectangle's transform has finite values and positive dimensions.
 * 
 * @param rectangle The rectangle's geometry to validate.
 * @param transform The rectangle's transform to validate.
 * 
 * @throws std::invalid_argument if the transform has non-finite values or non-positive dimensions.
 */
static void ValidateRectangle(const Rectangle2D& rectangle, const Transform& transform)
{
    if (!std::isfinite(transform.x) || !std::isfinite(transform.y) ||
        !std::isfinite(rectangle.x) || !std::isfinite(rectangle.y) ||
        !std::isfinite(rectangle.width) || !std::isfinite(rectangle.height) ||
        !std::isfinite(transform.rotation) || rectangle.width <= 0.0F || rectangle.height <= 0.0F) {
        throw std::invalid_argument("Collision rectangles require finite transforms and positive dimensions.");
    }
}

/**
 * Validates that a rectangle's transform has finite values and positive dimensions,
 * then returns the four corners in clockwise order starting from the top-left corner.
 * 
 * @param rectangle The rectangle's geometry to validate and extract corners from.
 * @param transform The rectangle's transform to validate and extract corners from.
 * 
 * @return An array of four Vector2D objects representing the corners of the rectangle
 * in clockwise order starting from the top-left corner.
 */
static std::array<Vector2D, 4> RectangleVertices(const Rectangle2D& rectangle, const Transform& transform)
{
    ValidateRectangle(rectangle, transform);
    return RectangleGeometry(
        TransformRectangle(rectangle, transform), transform.rotation
    ).Corners();
}

/**
 * Tests convex polygons with at least three vertices ordered around each boundary.
 * Convexity is assumed, not checked. 
 * 
 * All vertices and edge lengths must be finite and edges nonzero.
 * 
 * Vertices must be passed in consecutive order, e.g. clockwise or counterclockwise
 * around the polygon. The first and last vertices need not be the same.
 * 
 * The result describes moving a, leaving b fixed.
 * 
 * @param a The first convex polygon's vertices.
 * @param b The second convex polygon's vertices.
 * 
 * @return Contact information, or std::nullopt when separated.
 * 
 * @throws std::invalid_argument for non-finite vertices or edges with non-positive lengths.
 * 
 * Reference: https://dyn4j.org/2010/01/sat/
 */
static std::optional<Collision2D> DetectConvexCollision(
    std::span<const Vector2D> a, std::span<const Vector2D> b
)
{
    Collision2D collision{{}, std::numeric_limits<float>::infinity()};

    // How do we solve the problem of detecting contact between two convex polygons?
    // The Separating Axis Theorem (SAT) says that if a line exists that separates
    // the two polygons, they do not intersect. If no such line exists, they do intersect.
    // How do we find such a line? Well actually we don't need to find one if we can prove
    // that it does/does not exist. 
    //
    // I recommend following along with this with either MS Paint or pen and paper. 
    // 
    // First, reduce the two-dimensional question to a one-dimensional question.
    // Choose a line through the origin, pointing along a unit vector n.
    // For each vertex v, draw a line through v perpendicular to n. 
    // Where that line meets the axis is the projected point.
    //
    // To find the scalar projection of a vertex onto an axis, 
    // let a vertex v be distance r from the origin at angle phi.
    // Let the axis n have angle theta. Then the coordinates of the 
    // vertex and axis are:
    //
    //     v = (r cos(phi), r sin(phi))
    //     n = (cos(theta), sin(theta))
    //
    // The right triangle formed by dropping that perpendicular gives
    // us a parameter p, the distance along the axis to the projected point.
    // Note that we can recover the projected point along the axis by
    // multiplying p by the unit vector n.
    // How do we compute p? Well as we said, its the signed distance along the axis
    // to the projected point, which if we're looking at things from the angle
    // defined by the axis and the line from the origin to the vertex, is the
    // length of the adjacent side of the triangle. So we can use the cah 
    // part of soh cah toa to get p = r cos(phi - theta) as r is the hypotenuse
    // and phi - theta is the angle between the hypotenuse and the adjacent side.
    // Simplifying we see:
    //
    //     p = r cos(phi - theta)
    //       = r cos(phi)cos(theta) + r sin(phi)sin(theta)
    //       = v.x * n.x + v.y * n.y
    //       = v dot n
    //
    // So it can be a negative distance if the projected point is in the opposite
    // direction of the axis. 
    //
    // Along an edge of our polygon defined by the line segment from v0 to v1,
    // we can parameterize any point on that edge as
    // 
    //     v(t) = (1-t)*v0 + t*v1, where 0 <= t <= 1
    // 
    // Now as we explored previously, for those vertices v0 and v1, we can compute
    // their projections p0 and p1 onto the axis n. So we can then turn our
    // weighted average of the vertices into a weighted average of the projections:
    // 
    //     p(t) = (1-t)*p0 + t*p1,
    // 
    // Interior points of a convex polygon are weighted averages of its vertices. 
    // No point can project past the smallest or largest vertex projection. 
    // Thus two numbers describe the whole projection.
    // 
    // So we do not need to project every point along every edge
    // or every point inside the polygon. Projecting just the vertices
    // is enough to find the smallest and largest projected coordinates.
    //
    // The polygon's projection fills the interval [minimum, maximum].
    // This is what the following function computes: for each vertex,
    // calculate p = vertex dot axis, then retain the smallest and largest p.
    //
    // Calling this function for both polygons gives two intervals on the
    // same axis. We can then check whether those intervals have a gap.
    // A gap proves the polygons are separated along this direction.
    
    /**
     * Projects a convex polygon's vertices onto a given axis and returns the
     * minimum and maximum scalar projections along that axis.
     * 
     * @param vertices The vertices of the convex polygon to project.
     * @param axis The unit vector representing the axis onto which to project.
     * 
     * @return std::pair<float, float> A pair containing the minimum and maximum scalar projections.
     */
    const auto project = [](std::span<const Vector2D> vertices, Vector2D axis) {
        float minimum = std::numeric_limits<float>::infinity();
        float maximum = -std::numeric_limits<float>::infinity();
        for (Vector2D vertex : vertices) {
            const float projection = vertex.x * axis.x + vertex.y * axis.y;
            minimum = std::min(minimum, projection);
            maximum = std::max(maximum, projection);
        }
        return std::pair{minimum, maximum};
    };

    // Hold any questions for a bit, and try to understand the next three paragraphs.
    // 
    // If two polygons overlap at some point, that means there is some projection
    // of that point onto any axis that lies within both polygons' projected intervals.
    // 
    // Therefore, a gap between their projected intervals proves they cannot intersect.
    // So if ANY, and I stress ANY, axis exists where the projections do not overlap,
    // then the polygons are separated.
    // 
    // But overlap on just one axis doesn't really tell us much. If we could check 
    // every possible axis, we could prove that the polygons intersect. But there are
    // infinitely many axes, so we can't check them all. 
    // 
    // So now we can start asking questions. Specifically, which axes do we need to check? 
    //
    // For convex polygons, SAT says the normals of BOTH polygons' edges suffice.
    // You can just accept that as a fact, or as I will attempt to explain, consider the following.
    // 
    // Pick any point a inside or on polygon A, and any point b inside or on B.
    // Subtract their positions:
    //
    //     d = a - b
    //
    // Now imagine doing this for EVERY pair of points, not just the vertices.
    // Plot each resulting d as a point relative to the origin.
    // Together, these points fill a shape called the Minkowski difference.
    //
    // Why is this useful? Consider what it means for d to be (0, 0):
    //
    //     a - b = (0, 0)
    //         a = b
    //
    // It means A and B contain a point at exactly the same position.
    // Therefore, A and B intersect if and only if their difference shape
    // contains the origin. This includes touching at their boundaries.
    //
    // The difference shape is also convex. To see why, choose two of its points:
    //
    //     d0 = a0 - b0
    //     d1 = a1 - b1
    //
    // A point between them can be written as:
    //
    //     (1-t)*d0 + t*d1
    //       = ((1-t)*a0 + t*a1) - ((1-t)*b0 + t*b1)
    //
    // Since A and B are convex, those two parenthesized points remain inside
    // A and B respectively. Their difference therefore remains in our shape.
    //
    // We have now turned "do two polygons intersect?" into
    // "is the origin inside this one convex polygon?"
    //
    // Imagine extending each edge of this difference polygon into an infinite line.
    // Each line has an interior side containing the polygon, and an exterior side.
    // A point is inside the polygon exactly when it is on the interior side
    // of every edge line, or on the boundary.
    // 
    // Suppose the origin is outside the difference polygon.
    // Then at least one edge line has the origin on its exterior side,
    // while the entire difference polygon lies on its interior side.
    // Choose such an edge line.
    //
    // Choose a unit normal n perpendicular to this edge line,
    // pointing from the origin's side toward the polygon.
    // Project every difference point onto the axis through the origin
    // in direction n. All these scalar projections are strictly positive:
    //
    //     d dot n > 0
    //
    // Substitute d = a - b, recalling that a was any arbitrary point in A 
    // and b was any arbitrary point in B:
    //
    //     (a - b) dot n > 0
    //     a dot n - b dot n > 0
    //     a dot n > b dot n
    //
    // This holds for EVERY pair a and b. Even A's smallest projection is
    // greater than B's largest projection. Their projected intervals have a gap!
    //
    // But why can we use the ORIGINAL polygons' edge normals?
    // Why don't we have to construct this difference polygon first?
    //
    // Consider the boundary of the difference polygon furthest along some n.
    // To make d dot n as large as possible, we independently choose:
    //
    //     a with the largest a dot n
    //     b with the smallest b dot n
    //
    // If both choices identify a single vertex, their difference is just one point.
    // For this boundary to contain an entire edge, at least one choice must
    // allow movement along an edge of A or B without changing its projection.
    //
    // Movement along such an edge must be perpendicular to n:
    // otherwise moving along it would change the dot product.
    // So we're moving along an edge of A or B that has n as its normal.
    // Thus every edge normal of the difference polygon is also an edge normal
    // of A or B, possibly pointing in the opposite direction.
    //
    // Consequently, if the origin is outside the difference polygon,
    // an edge normal from A or B must reveal the separating gap.
    // We can test those normals directly, without building another polygon.
    //
    // That is what the following loops do: take each edge from each polygon,
    // construct its normal, and project BOTH polygons onto that axis.
    // A gap on any axis means separation. No gaps means contact.
    //
    // Opposite normals describe the same axis line. A rectangle contributes
    // only two distinct axes, but checking all its edges again is harmless
    // and lets this same loop handle other convex polygons.
    // 
    // If we really wanted, it is possible we could optimize it by 
    // checking if an edge's normal is already in the set of axes to check, 
    // but for now I don't think that is worth the extra complexity.

    for (const auto vertices : {a, b}) {
        for (std::size_t i = 0; i < vertices.size(); ++i) {
            // Subtract consecutive vertices to obtain the edge's displacement.
            // The modulo connects the final vertex back to the first.
            const Vector2D edge = vertices[(i + 1) % vertices.size()] - vertices[i];
            const float length = std::hypot(edge.x, edge.y);
            if (!std::isfinite(length) || length <= 0.0F) {
                throw std::invalid_argument("Collision polygon edges require finite, positive lengths.");
            }

            // For edge e = (dx, dy), choose the normal n = (-dy, dx).
            // Their dot product is dx*(-dy) + dy*dx = 0, so they are perpendicular.
            // Also, |n| = sqrt((-dy)^2 + dx^2) = |e|.
            // Dividing by the edge length therefore gives a unit normal.
            //
            // Without this division, the projection formula would give
            // v dot n = |v|*|n|*cos(phi-theta): all distances would be scaled by |n|.
            // Boolean overlap would still work, but comparing penetration depths
            // between axes from differently sized edges would give the wrong answer.

            const Vector2D axis = Vector2D{-edge.y, edge.x} / length;
            const auto [a_min, a_max] = project(a, axis);
            const auto [b_min, b_max] = project(b, axis);

            // If A's rightmost projection precedes B's leftmost, there is a gap.
            // The second comparison checks the opposite ordering.
            // We use strict < to report contact even when the polygons touch
            // at a single point or along an edge. 

            if (a_max < b_min || b_max < a_min) {
                return std::nullopt;
            }

            // The intervals intersect. Find how far A would need to move along
            // this axis to bring them to touching, while holding B fixed.
            // Translating a point by t*axis changes its projection by exactly t:
            //
            //     (v + t*axis) dot axis = v dot axis + t*(axis dot axis)
            //                           = p + t, since axis has unit length.
            //
            // To exit negatively, A's maximum must reach B's minimum:
            //     a_max + t = b_min  =>  t = b_min - a_max.
            // To exit positively, A's minimum must reach B's maximum:
            //     a_min + t = b_max  =>  t = b_max - a_min.
            //
            // Using the intersection's length instead of the exit distance
            // would fail for containment (like if A is inside B or vice versa).
            // With A = [1,3] and B = [0,10], their intersection has length 2.
            // Moving A by -2 gives [-1,1], which still intersects B.
            // The actual exits are -3, giving [-2,0], or +9, giving [10,12].

            const float negative_depth = a_max - b_min;
            const float positive_depth = b_max - a_min;
            const bool move_negative = negative_depth <= positive_depth;
            const float depth = move_negative ? negative_depth : positive_depth;

            // To remove penetration, we only need the projected intervals to stop
            // penetrating on one axis. So let's choose the axis requiring the least movement.
            // 
            // (might be an interesting experiment to see what happens if we choose
            // the axis requiring the most movement instead, or if we just essentially
            // pick a random axis by choosing the first one we find that has a gap.)
            // 
            // Moving A by normal * penetration_depth brings the shapes to touching,
            // anything further along that axis would separate them. The game can 
            // decide what to do.
            // 
            // Strict < retains the first axis when multiple exits are equally short.
            if (depth < collision.penetration_depth) {
                collision.normal = axis * (move_negative ? -1.0F : 1.0F);
                collision.penetration_depth = depth;
            }
        }
    }

    // At this point, we have checked every edge of both polygons. No gaps were found.
    return collision;
}

std::optional<Collision2D> DetectCollision(
    const Rectangle2D& a, const Transform& transform_a,
    const Rectangle2D& b, const Transform& transform_b
)
{
    const auto corners_a = RectangleVertices(a, transform_a);
    const auto corners_b = RectangleVertices(b, transform_b);
    return DetectConvexCollision(corners_a, corners_b);
}

std::optional<Collision2D> DetectCollision(
    const Triangle2D& a, const Transform& transform_a,
    const Triangle2D& b, const Transform& transform_b
)
{
    const auto world_a = TransformTriangle(a, transform_a);
    const auto world_b = TransformTriangle(b, transform_b);
    return DetectConvexCollision(world_a.vertices, world_b.vertices);
}

std::optional<Collision2D> DetectCollision(
    const Triangle2D& a, const Transform& transform_a,
    const Rectangle2D& b, const Transform& transform_b
)
{
    const auto world_a = TransformTriangle(a, transform_a);
    const auto corners_b = RectangleVertices(b, transform_b);
    return DetectConvexCollision(world_a.vertices, corners_b);
}

std::optional<Collision2D> DetectCollision(
    const Rectangle2D& a, const Transform& transform_a,
    const Triangle2D& b, const Transform& transform_b
)
{
    const auto corners_a = RectangleVertices(a, transform_a);
    const auto world_b = TransformTriangle(b, transform_b);
    return DetectConvexCollision(corners_a, world_b.vertices);
}

} // namespace svanes
