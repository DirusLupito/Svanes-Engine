#include "Piece.hpp"

#include <stdexcept>

namespace ghqcxx {

bool Piece::IsInfantry() const {
    return type == PieceType::BasicInfantry ||
           type == PieceType::ArmoredInfantry ||
           type == PieceType::ParachuteInfantry;
}

std::uint8_t Piece::GetFireRange() const {
    switch (type) {
    case PieceType::BasicArtillery:
    case PieceType::ArmoredArtillery:
        return 2;
    case PieceType::HeavyArtillery:
        return 3;
    default:
        return 0;
    }
}

MovementRules Piece::GetMovementRules() const {
    switch (type) {
    case PieceType::Headquarters:
    case PieceType::BasicInfantry:
    case PieceType::BasicArtillery:
    case PieceType::HeavyArtillery:
        return {.maxSteps = 1, .canParadrop = false};
    case PieceType::ArmoredInfantry:
    case PieceType::ArmoredArtillery:
        return {.maxSteps = 2, .canParadrop = false};
    case PieceType::ParachuteInfantry:
        return {.maxSteps = 1, .canParadrop = true};
    default:
        throw std::invalid_argument("Unknown GHQ piece type.");
    }
}

} // namespace ghqcxx
