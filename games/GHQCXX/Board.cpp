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

void Board::MovePiece(BoardCoordinate from, BoardCoordinate to) {
    GetPiece(to);
    for (const auto destination : GetMovementOptions(from)) {
        if (destination.column == to.column && destination.row == to.row) {
            pieces[to.row][to.column] = pieces[from.row][from.column];
            pieces[from.row][from.column].reset();
            return;
        }
    }
    throw std::invalid_argument("The selected piece cannot move to that square.");
}

const std::optional<Piece> &Board::GetPiece(BoardCoordinate position) const {
    if (position.column >= Size || position.row >= Size) {
        throw std::out_of_range(
            "GHQ board coordinates must be between 0 and 7.");
    }
    return pieces[position.row][position.column];
}

std::vector<Board::BoardCoordinate>
Board::GetMovementOptions(BoardCoordinate position) const {
    const auto &piece = GetPiece(position);
    if (!piece) {
        return {};
    }

    const auto rules = piece->GetMovementRules();
    std::vector<BoardCoordinate> destinations;
    const auto backRow = piece->team == Team::Red ? 0 : Size - 1;
    if (rules.canParadrop && position.row == backRow) {
        // A paradrop has no intervening squares to check. Nearby empty
        // squares are included here too, so they are only reported once.
        for (std::uint8_t row = 0; row < Size; ++row) {
            for (std::uint8_t column = 0; column < Size; ++column) {
                if (!GetPiece({column, row})) {
                    destinations.push_back({column, row});
                }
            }
        }
        return destinations;
    }

    for (std::int8_t rowStep = -1; rowStep <= 1; ++rowStep) {
        for (std::int8_t columnStep = -1; columnStep <= 1; ++columnStep) {
            if (rowStep == 0 && columnStep == 0) {
                continue;
            }

            // Keep intermediate coordinates signed so crossing the top or
            // left edge cannot wrap around to a positive board coordinate.
            std::int32_t column = position.column;
            std::int32_t row = position.row;
            for (std::uint8_t step = 0; step < rules.maxSteps; ++step) {
                column += columnStep;
                row += rowStep;
                if (column < 0 || column >= Size || row < 0 || row >= Size) {
                    break;
                }

                const BoardCoordinate destination{
                    .column = static_cast<std::uint8_t>(column),
                    .row = static_cast<std::uint8_t>(row)};
                if (GetPiece(destination)) {
                    break;
                }
                destinations.push_back(destination);
            }
        }
    }
    return destinations;
}

} // namespace ghqcxx
