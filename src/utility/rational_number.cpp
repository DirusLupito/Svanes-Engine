#include <svanes/utility/rational_number.hpp>

#include <limits>
#include <numeric>
#include <stdexcept>

namespace svanes {

/**
 * Helper function to safely multiply two uint64_t values,
 * throwing an overflow_error if the result exceeds the maximum representable
 * value.
 *
 * @param a The first uint64_t value to multiply.
 * @param b The second uint64_t value to multiply.
 *
 * @return The product of a and b if it does not overflow.
 *
 * @throws std::overflow_error if the product of a and b exceeds the maximum
 * representable value for uint64_t.
 */
static std::uint64_t CheckedMultiply(std::uint64_t a, std::uint64_t b) {
    if (b != 0 && a > std::numeric_limits<std::uint64_t>::max() / b) {
        throw std::overflow_error(
            "RationalNumber multiplication exceeds uint64_t.");
    }
    return a * b;
}

/**
 * Helper function to safely add two uint64_t values,
 * throwing an overflow_error if the result exceeds the maximum representable
 * value.
 *
 * @param a The first uint64_t value to add.
 * @param b The second uint64_t value to add.
 *
 * @return The sum of a and b if it does not overflow.
 *
 * @throws std::overflow_error if the sum of a and b exceeds the maximum
 * representable value for uint64_t.
 */
static std::uint64_t CheckedAdd(std::uint64_t a, std::uint64_t b) {
    if (a > std::numeric_limits<std::uint64_t>::max() - b) {
        throw std::overflow_error("RationalNumber addition exceeds uint64_t.");
    }
    return a + b;
}

RationalNumber::RationalNumber(std::uint64_t numerator,
                               std::uint64_t denominator)
    : numerator(numerator), denominator(denominator) {

    if (denominator == 0) {
        throw std::invalid_argument(
            "RationalNumber denominator cannot be zero.");
    }

    // Unlike my previous C# implementation, in C++ it seems I can
    // just use std::gcd to reduce the fraction to its simplest form.
    const std::uint64_t divisor = std::gcd(numerator, denominator);

    this->numerator /= divisor;
    this->denominator /= divisor;
}

// pojo slop

std::uint64_t RationalNumber::GetNumerator() const { return numerator; }

std::uint64_t RationalNumber::GetDenominator() const { return denominator; }

RationalNumber RationalNumber::operator+(const RationalNumber &other) const {
    // a/b + c/d = (a*(d/g) + c*(b/g)) / (b*(d/g)), where g=gcd(b,d).
    // so to add two fractions in a way that avoids overflow,
    // we can first find the gcd of the denominators,
    // then scale the numerators accordingly, and then add them together.


    const std::uint64_t common = std::gcd(denominator, other.denominator);
    const std::uint64_t left_scale = other.denominator / common;
    const std::uint64_t right_scale = denominator / common;
    const std::uint64_t sum =
        CheckedAdd(CheckedMultiply(numerator, left_scale),
                   CheckedMultiply(other.numerator, right_scale));

    // Now we have our sum = ((ad/g) + (cb/g)). We know our new denominator is
    // (b*(d/g)), which is (denominator * left_scale). Since g is the gcd of the
    // denominators, we know there are some integers u and v such that
    //     b = g * u,
    //     d = g * v,
    // Since we removed the common factor g from the denominators, we know that
    // u and v are coprime. Therefore, our rational sum is
    //     a/b + c/d = (av + cu) / (guv)
    // Now note that av + cu = sum, and guv = denominator * left_scale.
    // We want to continue to avoid overflow, so we should reduce the fraction
    // by the gcd of sum and (denominator * left_scale). But we don't need to
    // find the gcd of sum and (denominator * left_scale) directly. Consider the
    // following:
    //
    // Does any prime number p which can divide v also divide sum? If it did,
    // then it would divide av + cu. Well we know it definitely divides av. Does
    // it divide cu? Well it definitely DOESN'T divide u, because like we said,
    // u and v are coprime, so p cannot divide u.
    //
    // So that just leaves us with the question of whether p can divide c.
    // p dividing v means there is some integer m such that
    //     v = m * p
    // and p dividing c means there is some integer n such that
    //     c = n * p
    // Recall that
    //     d = g * v
    // So by substituting, we have
    //     d = g * m * p
    //     c/d = (np)/(gmp)
    // But this means that p divides both the numerator and denominator of c/d,
    // which contradicts the fact that c/d is in simplest form. And we assumed p
    // was a prime, so it cannot be 1. So if p divides v, it cannot divide c.
    // Therefore, p cannot divide sum.
    //
    // This same argument can be made for any prime factor of u. Therefore, the
    // gcd of sum and u is 1, and the gcd of sum and v is also 1. This tells us
    // that any common factor remaining in the sum must divide g, since both
    // operands were reduced. So the gcd of sum and (denominator * left_scale)
    // is the same as the gcd of sum and g, giving us a more efficient way to
    // reduce the fraction to its simplest form.

    const std::uint64_t reduction = std::gcd(sum, common);
    return RationalNumber{sum / reduction,
                          CheckedMultiply(denominator / reduction, left_scale)};
}

RationalNumber RationalNumber::operator/(const RationalNumber &other) const {
    if (other.numerator == 0) {
        throw std::invalid_argument("Cannot divide a RationalNumber by zero.");
    }

    // (a/b) / (c/d) = (a*d)/(b*c).
    // We already know gcd(a, b) = 1 and gcd(c, d) = 1,
    // so when finding the gcd of the numerator and denominator of the result,
    // we need only consider the gcd of a and c, and the gcd of b and d.
    // The former will tell us how much to reduce the numerator,
    // and the latter will tell us how much to reduce the denominator.

    const std::uint64_t first = std::gcd(numerator, other.numerator);
    const std::uint64_t second = std::gcd(other.denominator, denominator);
    return RationalNumber{
        CheckedMultiply(numerator / first, other.denominator / second),
        CheckedMultiply(denominator / second, other.numerator / first)};
}

std::uint64_t RationalNumber::GetWholePart() const {
    return numerator / denominator;
}

RationalNumber RationalNumber::GetFractionalPart() const {
    return RationalNumber{numerator % denominator, denominator};
}

} // namespace svanes
