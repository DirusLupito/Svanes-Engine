#include <svanes/deterministic_math.hpp>

#include <cmath>
#include <stdexcept>

namespace svanes {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;
constexpr double kHalfPi = 0.5 * kPi;

/**
 * Computes the sine of a finite angle from a Taylor polynomial, after reducing
 * the angle to [-pi/2, pi/2] where the polynomial is accurate.
 * @param radians The angle.
 * @return The sine of the angle.
 */
double SineOf(double radians) {
    double reduced = radians - std::floor(radians / kTwoPi + 0.5) * kTwoPi;
    if (reduced > kHalfPi) {
        reduced = kPi - reduced;
    } else if (reduced < -kHalfPi) {
        reduced = -kPi - reduced;
    }
    const double squared = reduced * reduced;
    double series = -1.0 / 39916800.0;
    series = 1.0 / 362880.0 + squared * series;
    series = -1.0 / 5040.0 + squared * series;
    series = 1.0 / 120.0 + squared * series;
    series = -1.0 / 6.0 + squared * series;
    series = 1.0 + squared * series;
    return reduced * series;
}

/**
 * @param radians The angle to check.
 * @throws std::invalid_argument if the angle is not finite.
 */
void RequireFinite(float radians) {
    if (!std::isfinite(radians)) {
        throw std::invalid_argument(
            "Deterministic trigonometry requires a finite angle.");
    }
}

} // namespace

float Length(float x, float y) { return std::sqrt(x * x + y * y); }

float Sin(float radians) {
    RequireFinite(radians);
    return static_cast<float>(SineOf(radians));
}

float Cos(float radians) {
    RequireFinite(radians);
    return static_cast<float>(SineOf(static_cast<double>(radians) + kHalfPi));
}

} // namespace svanes
