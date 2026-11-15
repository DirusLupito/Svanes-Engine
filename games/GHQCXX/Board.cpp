#include "Board.hpp"

#include <stdexcept>

namespace ghqcxx {

void Board::Initialize() {
    if (initialized) {
        throw std::logic_error("The GHQ board has already been initialized.");
    }

    initialized = true;

    // Starting according to the rulebook.
    PlacePiece({PieceType::Headquarters, Team::Red}, {0, 0});
    PlacePiece({PieceType::Headquarters, Team::Blue}, {7, 7});
    PlacePiece({PieceType::BasicArtillery, Team::Red}, {1, 0});
    PlacePiece({PieceType::BasicArtillery, Team::Blue}, {6, 7});

    for (std::uint8_t column = 0; column < 3; ++column) {
        PlacePiece({PieceType::BasicInfantry, Team::Red}, {column, 1});
        PlacePiece({PieceType::BasicInfantry, Team::Blue},
                   {static_cast<std::uint8_t>(Size - 1 - column), 6});
    }
}

void Board::PlacePiece(Piece piece, BoardCoordinate position) {
    if (!initialized) {
        throw std::logic_error(
            "Initialize the GHQ board before placing pieces.");
    }
    if (GetPiece(position)) {
        throw std::logic_error(
            "Cannot place a piece on an occupied GHQ square.");
    }

    if (piece.team != Team::Red && piece.team != Team::Blue) {
        throw std::invalid_argument("Unknown GHQ team.");
    }

    switch (piece.type) {
    case PieceType::Headquarters:
    case PieceType::BasicInfantry:
    case PieceType::ArmoredInfantry:
    case PieceType::ParachuteInfantry:
    case PieceType::BasicArtillery:
    case PieceType::HeavyArtillery:
    case PieceType::ArmoredArtillery:
        break;
    default:
        throw std::invalid_argument("Unknown GHQ piece type.");
    }

    pieces[position.row][position.column] = piece;
}

const std::optional<Board::Piece> &
Board::GetPiece(BoardCoordinate position) const {
    if (position.column >= Size || position.row >= Size) {
        throw std::out_of_range(
            "GHQ board coordinates must be between 0 and 7.");
    }
    return pieces[position.row][position.column];
}

} // namespace ghqcxx
