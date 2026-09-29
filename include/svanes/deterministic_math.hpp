#pragma once

namespace svanes {

/**
 * Gives the same result on every platform, unlike std::hypot.
 * @param x The horizontal component.
 * @param y The vertical component.
 * @return The length of the vector (x, y).
 */
float Length(float x, float y);

/**
 * Gives the same result on every platform, unlike std::sin.
 * @param radians The angle, any finite value.
 * @return The sine of the angle, within about 1e-7 of the exact value.
 * @throws std::invalid_argument if the angle is not finite.
 */
float Sin(float radians);

/**
 * Gives the same result on every platform, unlike std::cos.
 * @param radians The angle, any finite value.
 * @return The cosine of the angle, within about 1e-7 of the exact value.
 * @throws std::invalid_argument if the angle is not finite.
 */
float Cos(float radians);

} // namespace svanes
