#include <svanes/utility/rational_number.hpp>

#include <numeric>
#include <stdexcept>

namespace svanes {

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

} // namespace svanes
