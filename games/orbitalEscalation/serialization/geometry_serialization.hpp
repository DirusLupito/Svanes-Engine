#pragma once

#include "json.hpp"

#include <svanes/geometry/geometry.hpp>

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
svanes::Vector2D ReadPoint(const nlohmann::json &value);

/**
 * Writes a 2D point (Vector2D) to a JSON array.
 *
 * @param point The Vector2D to write.
 *
 * @return A JSON array representing the point.
 */
nlohmann::json WritePoint(svanes::Vector2D point);

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
svanes::Geometry2D ReadGeometry(const nlohmann::json &value);

/**
 * Writes a geometric shape (Primitive2D or CompositeShape2D) to JSON format.
 *
 * @param geometry The Geometry2D shape to write.
 *
 * @return A JSON object representing the geometric shape.
 */
nlohmann::json WriteGeometry(const svanes::Geometry2D &geometry);

/**
 * Appends a geometric shape (Primitive2D or CompositeShape2D) to a composite
 * shape (CompositeShape2D), either as a new part or in the case of a composite,
 * by appending its parts to the existing composite shape.
 *
 * @param collider The CompositeShape2D to which the shape will be appended.
 * @param geometry The Geometry2D shape to append.
 */
void AppendCollision(svanes::CompositeShape2D &collider,
                     const svanes::Geometry2D &geometry);
