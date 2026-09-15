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

private:

    // The non-negative numerator of the rational number.
    std::uint64_t numerator;

    // The positive denominator of the rational number.
    std::uint64_t denominator;
};

} // namespace svanes
