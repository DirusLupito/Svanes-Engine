#include <svanes/collision_system.hpp>

#include <svanes/circle_geometry.hpp>
#include <svanes/rectangle_geometry.hpp>
#include <svanes/triangle_geometry.hpp>
#include <svanes/geometry.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
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
 * Projects a convex polygon's vertices onto a given axis and returns the
 * minimum and maximum scalar projections along that axis.
 *
 * @param vertices The vertices of the convex polygon to project.
 * @param axis The unit vector representing the axis onto which to project.
 *
 * @return std::pair<float, float> A pair containing the minimum and maximum scalar projections.
 */
static std::pair<float, float> ProjectPolygon(std::span<const Vector2D> vertices, Vector2D axis)
{
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

    float minimum = std::numeric_limits<float>::infinity();
    float maximum = -std::numeric_limits<float>::infinity();
    for (Vector2D vertex : vertices) {
        const float projection = vertex.x * axis.x + vertex.y * axis.y;
        minimum = std::min(minimum, projection);
        maximum = std::max(maximum, projection);
    }
    return std::pair{minimum, maximum};
}

/**
 * Updates the collision information based on the projections of two convex polygons or
 * a convex polygon and a circle onto a given axis.
 * If the projections overlap, it calculates the penetration depth and updates the collision normal.
 * 
 * @param collision The current collision information to update.
 * @param axis The unit vector representing the axis onto which to project.
 * @param a_min The minimum scalar projection of the first polygon onto the axis.
 * @param a_max The maximum scalar projection of the first polygon onto the axis.
 * @param b_min The minimum scalar projection of the second polygon onto the axis.
 * @param b_max The maximum scalar projection of the second polygon onto the axis.
 * 
 * @return true if the projections overlap (indicating a potential collision), false otherwise.
 * 
 * @throws std::invalid_argument if any of the projections are not finite,
 * or if the penetration depth is not finite.
 */
static bool UpdateCollision(
    Collision2D& collision, Vector2D axis, float a_min, float a_max, float b_min, float b_max
)
{
    if (!std::isfinite(a_min) || !std::isfinite(a_max) || !std::isfinite(b_min) || !std::isfinite(b_max)) {
        throw std::invalid_argument("Collision projections must be finite.");
    }
    // If A's rightmost projection precedes B's leftmost, there is a gap.
    // The second comparison checks the opposite ordering.
    // We use strict < to report contact even when the polygons touch
    // at a single point or along an edge.

    if (a_max < b_min || b_max < a_min) {
        return false;
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
    if (!std::isfinite(depth)) {
        throw std::invalid_argument("Collision penetration depth must be finite.");
    }

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
    return true;
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
            const auto [a_min, a_max] = ProjectPolygon(a, axis);
            const auto [b_min, b_max] = ProjectPolygon(b, axis);

            if (!UpdateCollision(collision, axis, a_min, a_max, b_min, b_max)) {
                return std::nullopt;
            }
        }
    }

    // At this point, we have checked every edge of both polygons. No gaps were found.
    return collision;
}

/**
 * Tests a circle against a convex polygon along a given axis.
 * The circle is projected onto the axis as an interval centered at its projection
 * with a length equal to its diameter. The polygon is projected onto the axis
 * as an interval defined by its minimum and maximum projections. If the intervals
 * overlap, the collision information is updated with the penetration depth and normal.
 * 
 * @param circle The circle to test against the polygon.
 * @param polygon The vertices of the convex polygon to test against the circle.
 * @param axis The unit vector representing the axis onto which to project.
 * @param collision The current collision information to update.
 * 
 * @return true if the projections overlap (indicating a potential collision), false otherwise.
 * 
 * @throws std::invalid_argument if any of the projections are not finite,
 * or if the penetration depth is not finite.
 */
static bool TestCircleAxis(
    const Circle2D& circle, std::span<const Vector2D> polygon, Vector2D axis, Collision2D& collision
)
{

    // The projection of a circle along any axis is an interval centered at
    // the projection of its center, with a length equal to its diameter.

    const float projection = circle.x * axis.x + circle.y * axis.y;
    const auto [minimum, maximum] = ProjectPolygon(polygon, axis);
    return UpdateCollision(collision, axis, projection - circle.radius, projection + circle.radius, minimum, maximum);
}

/**
 * Detects collision between a circle and a convex polygon using the Separating Axis Theorem.
 * If a collision is found, it calculates the penetration depth and normal of the collision.
 * 
 * @param circle The circle to test against the polygon.
 * @param polygon The vertices of the convex polygon to test against the circle.
 * 
 * @return std::optional<Collision2D> The collision information if a collision is detected,
 * or std::nullopt if no collision is detected.
 * 
 * @throws std::invalid_argument if any of the projections are not finite,
 * or if the penetration depth is not finite.
 */
static std::optional<Collision2D> DetectCirclePolygonCollision(
    const Circle2D& circle, std::span<const Vector2D> polygon
)
{
    Collision2D collision{{}, std::numeric_limits<float>::infinity()};
    const Vector2D center{circle.x, circle.y};
    Vector2D closest;
    float closest_distance = std::numeric_limits<float>::infinity();

    // With the polygon-to-polygon SAT test, we only needed to check the
    // edge normals of the polygons. But a circle has no edges, or perhaps
    // infinitely many edges. So we cannot rely on the circle's edges to provide axes to test.
    // 
    // Instead, we can rely on the combination of the polygon's edge normals and one more:
    // an axis defined by the vector from the circle's center to the closest point on the polygon.
    // 
    // As for why, let's start with the earlier Minkowski difference argument I explained
    // for polygon-to-polygon collisions. Every point in a circle can be represented
    // as a 2D vector from the circle's center to a point on its boundary.
    // 
    // Let:
    // c - the circle's center as a 2D vector
    // r - the radius of the circle
    // p - a point anywhere in our convex polygon
    // 
    // Then what I am saying is that every point in the circle can be represented as:
    //
    //     c + u, ||u|| <= r
    //
    // Every difference between our arbitrary polygon point p and a point in the circle is then:
    //
    //     p - (c + u) = p - c - u
    // 
    // and so our set of points in the Minkowski difference is:
    //
    //     { p - c - u | p in polygon, ||u|| <= r }
    //
    // c is just some constant representing the circle's center, meaning that this shape is just
    // a translation of the shape given by
    //
    //    { p - u | p in polygon, ||u|| <= r }
    //
    // which is just the polygon minus a circle of radius r centered at the origin.
    // Notice that ||u|| <= r is satisfied by both +u and -u, so we can also write this as
    //
    //    { p + u | p in polygon, ||u|| <= r }
    //
    // So our set of points in the Minkowski difference is just every single point in the polygon
    // expanded by a circle of radius r. You can visualize this in your head by imagining taking
    // your polygon and a big marker with a circular tip whose radius is r, and tracing the outline
    // of the polygon with that marker, then filling in the interior. Edges will be expanded outward
    // by a distance of r, and corners will turn into circular arcs of radius r.
    // Call this our Minkowski sum of the polygon and a circle of radius r.
    // 
    // Now, getting back to the SAT argument, recall that the shapes intersect if and only if the origin
    // is contained in the Minkowski difference. That means we need to check if 0 is in the set given by
    // 
    //     { p + u | p in polygon, ||u|| <= r } - c
    //
    // which is equivalent to checking if c is in our Minkowski sum. How can we check if c is in the Minkowski sum? 
    // Well, rather than check if it is, suppose we assume it is not, i.e. the circle and polygon do not intersect.
    // Call:
    // q - the closest point in the polygon to c
    //
    // Then define
    // 
    //     d = ||c - q||,
    //     n = (c - q) / d
    // 
    // Now n is a unit vector pointing from the closest point on the polygon to the circle's center.
    // We also know that d > r because we assumed the circle and polygon do not intersect. 
    // 
    // Now we will show that a line through q perpendicular to n separates the two shapes.
    // Choose any other point p in the polygon. Convexity guarantees that every point given by
    // 
    //     q + t*(p - q), 0 <= t <= 1
    // 
    // is also in the polygon. Now since q is the closest point to c, we know that moving
    // from p to q cannot bring us any closer to c. So we can write the squared distance
    // from c to any point on the line segment from p to q as:
    //
    //     ||c - (q + t*(p - q))||^2 = ||(c - q) - t*(p - q)||^2
    //                               = ||c - q||^2 - 2*t*(c - q) dot (p - q) + t^2*||p - q||^2
    // 
    // If the term (c - q) dot (p - q) were positive, then for small t, 
    // the squared distance would be less than ||c - q||^2, meaning we would have found a point on the polygon
    // closer to c than q, contradicting the assumption that q is the closest point. Therefore,
    // we must have (c - q) dot (p - q) <= 0. If we then divide by d > 0, we have
    //
    //     n dot (p - q) <= 0
    //
    // which can be rewritten as
    //
    //     n dot p <= n dot q
    //
    // which holds for our arbitrary point p in the polygon. So the projection of the polygon onto the axis defined by n
    // attains its maximum at q dot n. Projecting the circle onto the same axis gives an interval centered at c dot n
    // with a length of 2*r. Solving our definition of n for c tells us that c = q + d*n, so
    // 
    //     c dot n - r = q dot n + d*n dot n - r = q dot n + d - r
    // 
    // Since the shapes do not intersect, we have d - r > 0, and so we know that
    // 
    //     c dot n - r > q dot n
    // 
    // So the circle's projection onto the axis defined by n lies entirely to the right of the polygon's projection.
    // Now how do we know if our axes comprised of the polygon's edge normals and the axis pointing from the circle's
    // center to the closest vertex of the polygon are sufficient to detect all collisions? Well we have a few cases:
    // 
    // 1. The circle center is inside the polygon. Then on every axis, the polygon's projection contains the circle's
    //   projection.
    // 2. The circle center is outside the polygon.
    //     a. The closest point on the polygon is a vertex. Then the axis from the circle's center to that vertex separates
    //        them according to the argument above.
    //     b. The closest point on the polygon is along an edge. Then we're already testing the edge's normal, and according
    //        to the argument above, that axis separates them.
    // 
    // So no matter what, all we have to do is check the polygon's edge normals and the axis from the circle's center to
    // the closest point on the polygon. If any of those axes separate the shapes, we know they do not intersect.
    // Otherwise, they must intersect.


    // Iterate over each vertex of the polygon to find the closest point to the circle's center.
    for (std::size_t i = 0; i < polygon.size(); ++i) {

        const Vector2D offset = polygon[i] - center;
        const float distance = std::hypot(offset.x, offset.y);

        if (!std::isfinite(distance)) {
            throw std::invalid_argument("Circle-to-polygon distances must be finite.");
        }

        if (distance < closest_distance) {
            closest = offset;
            closest_distance = distance;
        }

        // While we're already iterating over the polygon's vertices, we can also
        // build the edge normals during the same loop (after all, the number
        // of edges is equal to the number of vertices for a polygon).
        const Vector2D edge = polygon[(i + 1) % polygon.size()] - polygon[i];
        const float length = std::hypot(edge.x, edge.y);

        if (!std::isfinite(length) || length <= 0.0F) {
            throw std::invalid_argument("Collision polygon edges require finite, positive lengths.");
        }

        if (!TestCircleAxis(circle, polygon, Vector2D{-edge.y, edge.x} / length, collision)) {
            return std::nullopt;
        }
    }

    // At this point, no edge normals separated the shapes. All that's left is to check the axis from the circle's
    // center to the closest point on the polygon. If that axis separates them, we know they do not intersect.
    if (closest_distance > 0.0F && !TestCircleAxis(circle, polygon, closest / closest_distance, collision)) {
        return std::nullopt;
    }

    return collision;
}

/**
 * Detects contact between rectangles. Describes the nature or lack thereof of
 * the contact as both a normal and a penetration depth. 
 *
 * @param a The first rectangle's geometry.
 * @param transform_a The first rectangle's transform.
 * @param b The second rectangle's geometry.
 * @param transform_b The second rectangle's transform.
 * 
 * 
 * @return Contact information, or std::nullopt when separated. 
 * In the case that two different axes yield the same penetration depth,
 * the first axis encountered is the one returned.
 * 
 * @throws std::invalid_argument for non-finite transforms, nonpositive dimensions,
 * or rectangle edges that cannot be represented with a finite, positive length.
 */
static std::optional<Collision2D> DetectCollision(
    const Rectangle2D& a, const Transform& transform_a,
    const Rectangle2D& b, const Transform& transform_b
)
{
    const auto corners_a = RectangleVertices(a, transform_a);
    const auto corners_b = RectangleVertices(b, transform_b);
    return DetectConvexCollision(corners_a, corners_b);
}

/**
 * Detects contact between triangles. Describes the nature or lack thereof of
 * the contact as both a normal and a penetration depth.
 * 
 * @param a The first triangle's vertices.
 * @param b The second triangle's vertices.
 * 
 * @return Contact information, or std::nullopt when separated. 
 * In the case that two different axes yield the same penetration depth,
 * the first axis encountered is the one returned.
 *
 * @throws std::invalid_argument for triangle edges that cannot be represented with a finite, positive length.
 */
static std::optional<Collision2D> DetectCollision(
    const Triangle2D& a, const Transform& transform_a,
    const Triangle2D& b, const Transform& transform_b
)
{
    const auto world_a = TransformTriangle(a, transform_a);
    const auto world_b = TransformTriangle(b, transform_b);
    return DetectConvexCollision(world_a.vertices, world_b.vertices);
}

/**
 * Detects contact between a triangle and a rectangle. Describes the nature or lack thereof of
 * the contact as both a normal and a penetration depth. 
 *
 * @param a The triangle's vertices.
 * @param b The rectangle's geometry.
 * @param transform_b The rectangle's transform.
 * 
 * @return Contact information, or std::nullopt when separated. 
 * In the case that two different axes yield the same penetration depth,
 * the first axis encountered is the one returned.
 * 
 * @throws std::invalid_argument for non-finite transforms, nonpositive dimensions,
 * or triangle edges that cannot be represented with a finite, positive length.
 */
static std::optional<Collision2D> DetectCollision(
    const Triangle2D& a, const Transform& transform_a,
    const Rectangle2D& b, const Transform& transform_b
)
{
    const auto world_a = TransformTriangle(a, transform_a);
    const auto corners_b = RectangleVertices(b, transform_b);
    return DetectConvexCollision(world_a.vertices, corners_b);
}

/**
 * Detects contact between a rectangle and a triangle. Describes the nature or lack thereof of
 * the contact as both a normal and a penetration depth. 
 *
 * @param a The rectangle's geometry.
 * @param transform_a The rectangle's transform.
 * @param b The triangle's vertices.
 * 
 * @return Contact information, or std::nullopt when separated. 
 * In the case that two different axes yield the same penetration depth,
 * the first axis encountered is the one returned.
 * 
 * @throws std::invalid_argument for non-finite transforms, nonpositive dimensions,
 * or triangle edges that cannot be represented with a finite, positive length.
 */
static std::optional<Collision2D> DetectCollision(
    const Rectangle2D& a, const Transform& transform_a,
    const Triangle2D& b, const Transform& transform_b
)
{
    const auto corners_a = RectangleVertices(a, transform_a);
    const auto world_b = TransformTriangle(b, transform_b);
    return DetectConvexCollision(corners_a, world_b.vertices);
}

/**
 * Detects contact between two circles. Describes the nature or lack thereof of
 * the contact as both a normal and a penetration depth.
 *
 * @param a The first circle's geometry.
 * @param transform_a The first circle's transform.
 * @param b The second circle's geometry.
 * @param transform_b The second circle's transform.
 *
 * @return Contact information, or std::nullopt when separated.
 * In the case that two different axes yield the same penetration depth,
 * the first axis encountered is the one returned.
 *
 * @throws std::invalid_argument for non-finite transforms or non-positive radii.
 */
static std::optional<Collision2D> DetectCollision(
    const Circle2D& a, const Transform& transform_a,
    const Circle2D& b, const Transform& transform_b
)
{
    // Circle to circle collision detection is extremely simple. 
    // Just check if the sum of the radii is greater than the 
    // distance between the centers.

    const Circle2D world_a = TransformCircle(a, transform_a);
    const Circle2D world_b = TransformCircle(b, transform_b);

    const Vector2D offset{world_a.x - world_b.x, world_a.y - world_b.y};

    const float distance = std::hypot(offset.x, offset.y);
    const float radii = world_a.radius + world_b.radius;

    if (!std::isfinite(distance) || !std::isfinite(radii)) {
        throw std::invalid_argument("Circle collision distances must be finite.");
    }

    if (distance > radii) {
        return std::nullopt;
    }

    // As for the normal, we can just use the vector from the center of b to the center of a, normalized.
    // And the penetration depth is the sum of the radii minus the distance between the centers.

    return Collision2D{distance > 0.0F ? offset / distance : Vector2D{1.0F, 0.0F}, radii - distance};
}

/**
 * Detects contact between a circle and a rectangle. Describes the nature or lack thereof of
 * the contact as both a normal and a penetration depth.
 *
 * @param a The circle's geometry.
 * @param transform_a The circle's transform.
 * @param b The rectangle's geometry.
 * @param transform_b The rectangle's transform.
 *
 * @return Contact information, or std::nullopt when separated.
 * In the case that two different axes yield the same penetration depth,
 * the first axis encountered is the one returned.
 *
 * @throws std::invalid_argument for non-finite transforms, nonpositive dimensions,
 * or non-positive radii.
 */
static std::optional<Collision2D> DetectCollision(
    const Circle2D& a, const Transform& transform_a,
    const Rectangle2D& b, const Transform& transform_b
)
{
    const Circle2D circle = TransformCircle(a, transform_a);
    const auto polygon = RectangleVertices(b, transform_b);
    return DetectCirclePolygonCollision(circle, polygon);
}

/**
 * Detects contact between a rectangle and a circle. Describes the nature or lack thereof of
 * the contact as both a normal and a penetration depth.
 *
 * @param a The rectangle's geometry.
 * @param transform_a The rectangle's transform.
 * @param b The circle's geometry.
 * @param transform_b The circle's transform.
 *
 * @return Contact information, or std::nullopt when separated.
 * In the case that two different axes yield the same penetration depth,
 * the first axis encountered is the one returned.
 *
 * @throws std::invalid_argument for non-finite transforms, nonpositive dimensions,
 * or non-positive radii.
 */
static std::optional<Collision2D> DetectCollision(
    const Rectangle2D& a, const Transform& transform_a,
    const Circle2D& b, const Transform& transform_b
)
{
    auto collision = DetectCollision(b, transform_b, a, transform_a);
    if (collision) {
        collision->normal = collision->normal * -1.0F;
    }
    return collision;
}

/**
 * Detects contact between a circle and a triangle. Describes the nature or lack thereof of
 * the contact as both a normal and a penetration depth.
 *
 * @param a The circle's geometry.
 * @param transform_a The circle's transform.
 * @param b The triangle's vertices.
 * @param transform_b The triangle's transform.
 *
 * @return Contact information, or std::nullopt when separated.
 * In the case that two different axes yield the same penetration depth,
 * the first axis encountered is the one returned.
 *
 * @throws std::invalid_argument for non-finite transforms, non-positive radii,
 * or triangle edges that cannot be represented with a finite, positive length.
 */
static std::optional<Collision2D> DetectCollision(
    const Circle2D& a, const Transform& transform_a,
    const Triangle2D& b, const Transform& transform_b
)
{
    const Circle2D circle = TransformCircle(a, transform_a);
    const auto polygon = TransformTriangle(b, transform_b);
    return DetectCirclePolygonCollision(circle, polygon.vertices);
}

/**
 * Detects contact between a triangle and a circle. Describes the nature or lack thereof of
 * the contact as both a normal and a penetration depth.
 *
 * @param a The triangle's vertices.
 * @param transform_a The triangle's transform.
 * @param b The circle's geometry.
 * @param transform_b The circle's transform.
 *
 * @return Contact information, or std::nullopt when separated.
 * In the case that two different axes yield the same penetration depth,
 * the first axis encountered is the one returned.
 *
 * @throws std::invalid_argument for non-finite transforms, non-positive radii,
 * or triangle edges that cannot be represented with a finite, positive length.
 */
static std::optional<Collision2D> DetectCollision(
    const Triangle2D& a, const Transform& transform_a,
    const Circle2D& b, const Transform& transform_b
)
{
    auto collision = DetectCollision(b, transform_b, a, transform_a);
    if (collision) {
        collision->normal = collision->normal * -1.0F;
    }
    return collision;
}

/**
 * Detects all collisions between two primitive 2D shapes, which may be of different types (e.g., circle, rectangle, triangle).
 * The function uses std::visit to handle the different shape types and calls the appropriate collision detection function for
 * for each pair of shapes. If a collision is detected, it is added to the collisions vector.
 * 
 * @param a The first primitive shape to test for collisions.
 * @param transform_a The transform to apply to the first shape.
 * @param b The second primitive shape to test for collisions.
 * @param transform_b The transform to apply to the second shape.
 * @param collisions A vector to store the detected collisions.
 */
static std::optional<Collision2D> DetectCollision(
    const ConvexPolygon2D& a, const Transform& transform_a,
    const Rectangle2D& b, const Transform& transform_b
)
{
    const auto world_a = TransformConvexPolygon(a, transform_a);
    const auto world_b = RectangleVertices(b, transform_b);
    return DetectConvexCollision(world_a.Vertices(), world_b);
}

/**
 * Detects all collisions between two primitive 2D shapes, which may be of different types (e.g., circle, rectangle, triangle).
 * The function uses std::visit to handle the different shape types and calls the appropriate collision detection function for
 * for each pair of shapes. If a collision is detected, it is added to the collisions vector.
 * 
 * @param a The first primitive shape to test for collisions.
 * @param transform_a The transform to apply to the first shape.
 * @param b The second primitive shape to test for collisions.
 * @param transform_b The transform to apply to the second shape.
 * @param collisions A vector to store the detected collisions.
 */
static std::optional<Collision2D> DetectCollision(
    const ConvexPolygon2D& a, const Transform& transform_a,
    const Triangle2D& b, const Transform& transform_b
)
{
    const auto world_a = TransformConvexPolygon(a, transform_a);
    const auto world_b = TransformTriangle(b, transform_b);
    return DetectConvexCollision(world_a.Vertices(), world_b.vertices);
}

/**
 * Detects all collisions between two primitive 2D shapes, which may be of different types (e.g., circle, rectangle, triangle).
 * The function uses std::visit to handle the different shape types and calls the appropriate collision detection function for
 * for each pair of shapes. If a collision is detected, it is added to the collisions vector.
 * 
 * @param a The first primitive shape to test for collisions.
 * @param transform_a The transform to apply to the first shape.
 * @param b The second primitive shape to test for collisions.
 * @param transform_b The transform to apply to the second shape.
 * @param collisions A vector to store the detected collisions.
 */
static std::optional<Collision2D> DetectCollision(
    const ConvexPolygon2D& a, const Transform& transform_a,
    const ConvexPolygon2D& b, const Transform& transform_b
)
{
    const auto world_a = TransformConvexPolygon(a, transform_a);
    const auto world_b = TransformConvexPolygon(b, transform_b);
    return DetectConvexCollision(world_a.Vertices(), world_b.Vertices());
}

/**
 * Detects all collisions between a circle and a convex polygon. The function transforms both shapes into world space
 * using their respective transforms and then calls the DetectCirclePolygonCollision function to check for collisions.
 * 
 * @param a The circle to test for collisions.
 * @param transform_a The transform to apply to the circle.
 * @param b The convex polygon to test for collisions.
 * @param transform_b The transform to apply to the convex polygon.
 * 
 * @return std::optional<Collision2D> The collision information if a collision is detected,
 * or std::nullopt if no collision is detected.
 */
static std::optional<Collision2D> DetectCollision(
    const Circle2D& a, const Transform& transform_a,
    const ConvexPolygon2D& b, const Transform& transform_b
)
{
    const auto circle = TransformCircle(a, transform_a);
    const auto polygon = TransformConvexPolygon(b, transform_b);
    return DetectCirclePolygonCollision(circle, polygon.Vertices());
}

/**
 * Detects all collisions between a rectangle and a convex polygon. The function transforms both shapes into world space
 * using their respective transforms and then calls the DetectConvexCollision function to check for collisions.
 * 
 * @param a The rectangle to test for collisions.
 * @param transform_a The transform to apply to the rectangle.
 * @param b The convex polygon to test for collisions.
 * @param transform_b The transform to apply to the convex polygon.
 * 
 * @return std::optional<Collision2D> The collision information if a collision is detected,
 * or std::nullopt if no collision is detected.
 */
static std::optional<Collision2D> DetectCollision(
    const Rectangle2D& a, const Transform& transform_a,
    const ConvexPolygon2D& b, const Transform& transform_b
)
{
    auto collision = DetectCollision(b, transform_b, a, transform_a);
    if (collision) {
        collision->normal = collision->normal * -1.0F;
    }
    return collision;
}

/**
 * Detects all collisions between a triangle and a convex polygon. The function transforms both shapes into world space
 * using their respective transforms and then calls the DetectConvexCollision function to check for collisions.
 * 
 * @param a The triangle to test for collisions.
 * @param transform_a The transform to apply to the triangle.
 * @param b The convex polygon to test for collisions.
 * @param transform_b The transform to apply to the convex polygon.
 * 
 * @return std::optional<Collision2D> The collision information if a collision is detected,
 * or std::nullopt if no collision is detected.
 */
static std::optional<Collision2D> DetectCollision(
    const Triangle2D& a, const Transform& transform_a,
    const ConvexPolygon2D& b, const Transform& transform_b
)
{
    auto collision = DetectCollision(b, transform_b, a, transform_a);
    if (collision) {
        collision->normal = collision->normal * -1.0F;
    }
    return collision;
}

/**
 * Detects all collisions between a convex polygon and a circle. The function transforms both shapes into world space
 * using their respective transforms and then calls the DetectCirclePolygonCollision function to check for collisions.
 * 
 * @param a The convex polygon to test for collisions.
 * @param transform_a The transform to apply to the convex polygon.
 * @param b The circle to test for collisions.
 * @param transform_b The transform to apply to the circle.
 * 
 * @return std::optional<Collision2D> The collision information if a collision is detected,
 * or std::nullopt if no collision is detected.
 */
static std::optional<Collision2D> DetectCollision(
    const ConvexPolygon2D& a, const Transform& transform_a,
    const Circle2D& b, const Transform& transform_b
)
{
    auto collision = DetectCollision(b, transform_b, a, transform_a);
    if (collision) {
        collision->normal = collision->normal * -1.0F;
    }
    return collision;
}

static void AppendCollisions(
    const Primitive2D& a, const Transform& transform_a,
    const Primitive2D& b, const Transform& transform_b,
    std::vector<Collision2D>& collisions
)
{
    const auto collision = std::visit([&](const auto& shape_a, const auto& shape_b) {
        return DetectCollision(shape_a, transform_a, shape_b, transform_b);
    }, a, b);
    if (collision) {
        collisions.push_back(*collision);
    }
}

/**
 * Detects all collisions between a composite 2D shape, which may contain multiple parts, and a primitive 2D shape.
 * The function iterates through each part of the composite shape and checks for collisions with the primitive shape,
 * applying the appropriate transforms to each part.
 * 
 * @param a The composite shape to test for collisions.
 * @param transform_a The transform to apply to the composite shape.
 * @param b The primitive shape to test for collisions.
 * @param transform_b The transform to apply to the primitive shape.
 * @param collisions A vector to store the detected collisions.
 */
static void AppendCollisions(
    const CompositeShape2D& a, const Transform& transform_a,
    const Primitive2D& b, const Transform& transform_b,
    std::vector<Collision2D>& collisions
)
{
    for (const GeometryPart2D& part : a.parts) {
        AppendCollisions(part.shape, ComposeTransforms(transform_a, part.transform), b, transform_b, collisions);
    }
}

/**
 * Detects all collisions between a primitive 2D shape and a composite 2D shape, which may contain multiple parts.
 * The function iterates through each part of the composite shape and checks for collisions with the primitive shape,
 * applying the appropriate transforms to each part.
 * 
 * @param a The primitive shape to test for collisions.
 * @param transform_a The transform to apply to the primitive shape.
 * @param b The composite shape to test for collisions.
 * @param transform_b The transform to apply to the composite shape.
 * @param collisions A vector to store the detected collisions.
 */
static void AppendCollisions(
    const Primitive2D& a, const Transform& transform_a,
    const CompositeShape2D& b, const Transform& transform_b,
    std::vector<Collision2D>& collisions
)
{
    for (const GeometryPart2D& part : b.parts) {
        AppendCollisions(a, transform_a, part.shape, ComposeTransforms(transform_b, part.transform), collisions);
    }
}

/**
 * Detects all collisions between two composite 2D shapes, which may contain multiple parts.
 * The function iterates through each part of the first composite shape and checks for collisions
 * with the second composite shape, applying the appropriate transforms to each part.
 * 
 * @param a The first composite shape to test for collisions.
 * @param transform_a The transform to apply to the first composite shape.
 * @param b The second composite shape to test for collisions.
 * @param transform_b The transform to apply to the second composite shape.
 * @param collisions A vector to store the detected collisions.
 */
static void AppendCollisions(
    const CompositeShape2D& a, const Transform& transform_a,
    const CompositeShape2D& b, const Transform& transform_b,
    std::vector<Collision2D>& collisions
)
{
    for (const GeometryPart2D& part : a.parts) {
        AppendCollisions(part.shape, ComposeTransforms(transform_a, part.transform), b, transform_b, collisions);
    }
}

std::vector<Collision2D> DetectCollisions(
    const Geometry2D& a, const Transform& transform_a,
    const Geometry2D& b, const Transform& transform_b
)
{
    std::vector<Collision2D> collisions;

    // we need to delegate to the appropriate overload of AppendCollisions based on the types of a and b,
    // (either primitive or composite).
    std::visit([&](const auto& shape_a, const auto& shape_b) {
        AppendCollisions(shape_a, transform_a, shape_b, transform_b, collisions);
    }, a, b);
    return collisions;
}

} // namespace svanes
