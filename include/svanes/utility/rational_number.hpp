#pragma once

#include <cstdint>

namespace svanes {

/**
 * Represents a non-negative rational number as a numerator and denominator.
 * The numerator is non-negative, and the denominator is positive. Zero is
 * represented as 0/1. The rational number is always in its simplest form, with
 * no common factors between the numerator and denominator.
 */
class RationalNumber {
public:
    /**
     * Constructs a RationalNumber with the given numerator and denominator.
     * The denominator must be positive, and the numerator must be non-negative.
     * The rational number is automatically reduced to its simplest form.
     *
     * @param numerator The non-negative numerator of the rational number.
     * @param denominator The positive denominator of the rational number.
     *
     * @throws std::invalid_argument if the denominator is zero.
     */
    RationalNumber(std::uint64_t numerator, std::uint64_t denominator = 1);

    /**
     * Returns the numerator of the rational number.
     *
     * @return The non-negative numerator.
     */
    std::uint64_t GetNumerator() const;

    /**
     * Returns the denominator of the rational number.
     *
     * @return The positive denominator.
     */
    std::uint64_t GetDenominator() const;

    /**
     * Adds two fractions exactly and returns the result in simplest form.
     * The operands are unchanged on failure.
     *
     * @param other The other RationalNumber to add to this one.
     *
     * @return The sum of the two RationalNumbers in simplest form.
     *
     * @throws std::overflow_error if the result cannot be represented as a
     * RationalNumber.
     */
    RationalNumber operator+(const RationalNumber &other) const;

    /**
     * Divides by another fraction, cancelling common factors before
     * multiplying. Throws std::invalid_argument for division by zero, or
     * std::overflow_error if the result cannot be represented. The operands are
     * unchanged on failure.
     */
    RationalNumber operator/(const RationalNumber &other) const;

    /**
     * Returns the whole part of the rational number, discarding any fractional
     * part. For example, 15/2 returns 7.
     *
     * @return The whole part of the rational number.
     */
    std::uint64_t GetWholePart() const;

    /**
     * Returns the fractional part of the rational number, discarding any whole
     * part. For example, 15/2 returns 1/2.
     *
     * @return The fractional part of the rational number in simplest form.
     */
    RationalNumber GetFractionalPart() const;

private:
    // The non-negative numerator of the rational number.
    std::uint64_t numerator;

    // The positive denominator of the rational number.
    std::uint64_t denominator;
};

} // namespace svanes
