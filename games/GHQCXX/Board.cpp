#include "Board.hpp"

#include "GameMessage.hpp"

#include <svanes/network/message_serialization.hpp>

#include <limits>
#include <stdexcept>
#include <utility>

namespace ghqcxx {

// Number of basic infantry each team starts with on the board.
static constexpr std::uint8_t StartingInfantryCount = 3;

// Red's starting infantry row. Blue uses the mirrored row.
static constexpr std::uint8_t StartingInfantryRow = 1;

// Red's starting basic artillery column. Blue uses the mirrored column.
static constexpr std::uint8_t StartingArtilleryColumn = 1;

// Pieces left in reserve after the starting formation is placed, per team.
static constexpr std::array StartingReserveCounts{
    std::pair{PieceType::Headquarters, std::uint8_t{0}},
    std::pair{PieceType::BasicInfantry, std::uint8_t{5}},
    std::pair{PieceType::ArmoredInfantry, std::uint8_t{3}},
    std::pair{PieceType::ParachuteInfantry, std::uint8_t{1}},
    std::pair{PieceType::BasicArtillery, std::uint8_t{2}},
    std::pair{PieceType::HeavyArtillery, std::uint8_t{1}},
    std::pair{PieceType::ArmoredArtillery, std::uint8_t{1}}};

// The byte value written for both the type and team of an empty square.
static constexpr std::uint8_t EmptySquare =
    std::numeric_limits<std::uint8_t>::max();

// The size of a complete board state message in bytes.
static constexpr std::size_t BoardMessageSize =
    sizeof(GameMessageType) +
    Board::Size * Board::Size * (sizeof(Piece::type) + sizeof(Piece::team)) +
    TeamCount * PieceTypeCount * sizeof(std::uint8_t);

void Board::Initialize() {
    if (initialized) {
        throw std::logic_error("The GHQ board has already been initialized.");
    }

    initialized = true;

    // Starting according to the rulebook.
    PlacePiece({PieceType::Headquarters, Team::Red}, {0, 0});
    PlacePiece({PieceType::Headquarters, Team::Blue}, {Size - 1, Size - 1});
    PlacePiece({PieceType::BasicArtillery, Team::Red},
               {StartingArtilleryColumn, 0});
    PlacePiece({PieceType::BasicArtillery, Team::Blue},
               {Size - 1 - StartingArtilleryColumn, Size - 1});

    for (std::uint8_t column = 0; column < StartingInfantryCount; ++column) {
        PlacePiece({PieceType::BasicInfantry, Team::Red},
                   {column, StartingInfantryRow});
        PlacePiece({PieceType::BasicInfantry, Team::Blue},
                   {static_cast<std::uint8_t>(Size - 1 - column),
                    Size - 1 - StartingInfantryRow});
    }

    for (auto &reserve : reserves) {
        for (const auto &[type, count] : StartingReserveCounts) {
            reserve[static_cast<std::size_t>(type)] = count;
        }
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
    throw std::invalid_argument(
        "The selected piece cannot move to that square.");
}

svanes::NetworkMessage Board::ToMessage() const {
    svanes::MessageWriter writer;
    writer.WriteUint8(static_cast<std::uint8_t>(GameMessageType::BoardState));
    for (const auto &row : pieces) {
        for (const auto &piece : row) {
            writer.WriteUint8(piece ? static_cast<std::uint8_t>(piece->type)
                                    : EmptySquare);
            writer.WriteUint8(piece ? static_cast<std::uint8_t>(piece->team)
                                    : EmptySquare);
        }
    }
    // Reserve bytes follow the squares, ordered by Team, then PieceType.
    for (const auto &reserve : reserves) {
        for (const auto count : reserve) {
            writer.WriteUint8(count);
        }
    }
    return writer.Finish();
}

Board Board::FromMessage(const svanes::NetworkMessage &message) {
    if (message.bytes.size() != BoardMessageSize) {
        throw std::invalid_argument("Invalid GHQ board packet size.");
    }
    if (GetGameMessageType(message) != GameMessageType::BoardState) {
        throw std::invalid_argument("Expected a GHQ board state packet.");
    }

    svanes::MessageReader reader(message);
    reader.ReadUint8();
    Board board;
    // Restore the transmitted state without applying the starting formation.
    // The caller receives the board only after all fields validate.
    board.initialized = true;
    for (std::uint8_t row = 0; row < Size; ++row) {
        for (std::uint8_t column = 0; column < Size; ++column) {
            const auto type = reader.ReadUint8();
            const auto team = reader.ReadUint8();
            if (type == EmptySquare) {
                if (team != EmptySquare) {
                    throw std::invalid_argument(
                        "An empty GHQ square cannot have a team.");
                }
            } else {
                board.PlacePiece(
                    {static_cast<PieceType>(type), static_cast<Team>(team)},
                    {column, row});
            }
        }
    }
    for (auto &reserve : board.reserves) {
        for (auto &count : reserve) {
            count = reader.ReadUint8();
        }
    }
    return board;
}

const std::optional<Piece> &Board::GetPiece(BoardCoordinate position) const {
    if (position.column >= Size || position.row >= Size) {
        throw std::out_of_range(
            "GHQ board coordinates must be between 0 and 7.");
    }
    return pieces[position.row][position.column];
}

std::uint8_t Board::GetReserveCount(Piece piece) const {
    return reserves.at(static_cast<std::size_t>(piece.team))
        .at(static_cast<std::size_t>(piece.type));
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
