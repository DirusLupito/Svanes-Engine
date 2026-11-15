#include "Piece.hpp"

#include <stdexcept>

namespace ghqcxx {

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
