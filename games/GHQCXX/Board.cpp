#include "Board.hpp"

#include "GameMessage.hpp"

#include <svanes/network/message_serialization.hpp>

#include <algorithm>
#include <limits>
#include <ranges>
#include <stdexcept>
#include <utility>

namespace ghqcxx {

// The number of actions a player may take in one turn.
static constexpr std::uint8_t ActionsPerTurn = 3;

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

// The byte value written for each field of an empty square.
static constexpr std::uint8_t EmptySquare =
    std::numeric_limits<std::uint8_t>::max();

// The size of a complete board state message in bytes.
static constexpr std::size_t BoardMessageSize =
    sizeof(GameMessageType) +
    Board::Size * Board::Size *
        (sizeof(Piece::type) + sizeof(Piece::team) + sizeof(Piece::facing) +
         sizeof(Piece::action)) +
    TeamCount * PieceTypeCount * sizeof(std::uint8_t) + sizeof(std::uint32_t);

// Column and row steps in Facing order, clockwise from north.
static constexpr std::array<std::pair<std::int8_t, std::int8_t>, 8> FacingSteps{
    {{0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}}};

// The directions in which infantry can engage an opponent.
static constexpr std::array InfantryFacings{Facing::North, Facing::East,
                                            Facing::South, Facing::West};

/**
 * Finds a square along a piece's facing.
 *
 * @param from The starting square.
 * @param facing The direction to follow.
 * @param distance The number of squares to travel.
 * @return The destination, or no value if it falls outside the board.
 */
static std::optional<Board::BoardCoordinate>
SquareInDirection(Board::BoardCoordinate from, Facing facing,
                  std::uint8_t distance = 1) {
    const auto [dx, dy] = FacingSteps[static_cast<std::size_t>(facing)];
    const auto column = from.column + dx * distance;
    const auto row = from.row + dy * distance;
    if (column < 0 || column >= Board::Size || row < 0 || row >= Board::Size) {
        return std::nullopt;
    }
    return Board::BoardCoordinate{static_cast<std::uint8_t>(column),
                                  static_cast<std::uint8_t>(row)};
}

/**
 * Finds the facing for a piece looking back at its opponent.
 *
 * @param facing The opponent's facing.
 * @return The direction half a turn away.
 */
static Facing OppositeFacing(Facing facing) {
    return static_cast<Facing>(
        (static_cast<std::size_t>(facing) + FacingSteps.size() / 2) %
        FacingSteps.size());
}

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

bool Board::HasActionsRemaining(Team team) const {
    return GetActionsRemaining(team) > 0;
}

std::uint8_t Board::GetActionsRemaining(Team team) const {
    const auto used = std::ranges::count_if(
        pieces | std::views::join, [team](const auto &piece) {
            return piece && piece->team == team &&
                   piece->action != ActionState::Ready;
        });
    return used < ActionsPerTurn
               ? static_cast<std::uint8_t>(ActionsPerTurn - used)
               : 0;
}

void Board::EndTurn() {
    ++turnNumber;
    const auto activeTeam = GetActiveTeam();
    const auto bombardment = GetBombardment(activeTeam);
    for (std::uint8_t row = 0; row < Size; ++row) {
        for (std::uint8_t column = 0; column < Size; ++column) {
            auto &piece = pieces[row][column];
            if (!piece) {
                continue;
            }
            // Artillery captures at the start of its own team's turn.
            if (piece->team != activeTeam && bombardment[row][column]) {
                piece.reset();
            } else {
                piece->action = ActionState::Ready;
            }
        }
    }
    UpdateEngagements();
}

void Board::FinishArtilleryMove() {
    for (auto &piece : pieces | std::views::join) {
        if (piece && piece->action == ActionState::ChoosingFacing) {
            piece->action = ActionState::Exhausted;
        }
    }
}

/**
 * Checks the state of a piece before adding it to the board.
 *
 * @param piece The piece to check.
 * @throws std::invalid_argument if the piece has invalid state.
 */
static void ValidatePiece(Piece piece) {
    if (piece.team != Team::Red && piece.team != Team::Blue) {
        throw std::invalid_argument("Unknown GHQ team.");
    }

    if (piece.facing > Facing::NorthWest) {
        throw std::invalid_argument("Invalid GHQ piece facing.");
    }

    if (piece.action > ActionState::Exhausted ||
        (piece.action == ActionState::ChoosingFacing &&
         piece.GetFireRange() == 0)) {
        throw std::invalid_argument("Invalid GHQ piece action state.");
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

    ValidatePiece(piece);
    pieces[position.row][position.column] = piece;
    if (piece.IsInfantry()) {
        UpdateEngagements();
    }
}

bool Board::CanPlaceFromReserve(Piece piece, BoardCoordinate position) const {
    const auto count = GetReserveCount(piece);
    const auto backRow = piece.team == Team::Red ? 0 : Size - 1;
    return count > 0 && piece.type != PieceType::Headquarters &&
           position.row == backRow && position.column < Size &&
           !GetPiece(position) && HasActionsRemaining(piece.team);
}

void Board::PlaceFromReserve(Piece piece, BoardCoordinate position) {
    if (!CanPlaceFromReserve(piece, position)) {
        throw std::invalid_argument(
            "The reserve piece cannot be placed on that square.");
    }
    piece.action = ActionState::Exhausted;
    PlacePiece(piece, position);
    FinishArtilleryMove();
    SetReserveCount(piece, GetReserveCount(piece) - 1);
}

void Board::MovePiece(BoardCoordinate from, BoardCoordinate to,
                      std::optional<Facing> facing) {
    auto piece = GetPiece(from);
    GetPiece(to);
    if (!piece) {
        throw std::invalid_argument(
            "There is no piece on the selected square.");
    }

    if (facing && *facing > Facing::NorthWest) {
        throw std::invalid_argument("Invalid facing for the selected piece.");
    }

    if (from == to && facing) {
        // Engaged infantry face their opponent.
        if (piece->IsInfantry()) {
            if (!GetEngagedOpponent(from)) {
                pieces[from.row][from.column]->facing = *facing;
            }
            return;
        }

        if (piece->action == ActionState::Exhausted) {
            throw std::invalid_argument(
                "The selected piece has already acted.");
        }

        if (piece->action == ActionState::Ready && piece->facing == *facing) {
            return;
        }

        if (piece->action == ActionState::Ready &&
            !HasActionsRemaining(piece->team)) {
            throw std::invalid_argument("The team has used all three actions.");
        }
    } else if (!CanMovePiece(from, to)) {
        throw std::invalid_argument(
            "The selected piece cannot move to that square.");
    }

    FinishArtilleryMove();
    piece->action = !facing && piece->GetFireRange() > 0
                        ? ActionState::ChoosingFacing
                        : ActionState::Exhausted;
    piece->facing = facing.value_or(piece->facing);
    pieces[from.row][from.column].reset();
    pieces[to.row][to.column] = piece;
    if (piece->IsInfantry()) {
        UpdateEngagements();
    }
}

bool Board::CanMovePiece(BoardCoordinate from, BoardCoordinate to) const {
    return std::ranges::contains(GetMovementOptions(from), to);
}

std::vector<std::pair<Board::BoardCoordinate, Facing>>
Board::GetFireOptions(BoardCoordinate position) const {
    const auto &piece = GetPiece(position);
    const auto range = piece ? piece->GetFireRange() : 0;
    std::vector<std::pair<BoardCoordinate, Facing>> options;
    for (std::size_t direction = 0; direction < FacingSteps.size();
         ++direction) {
        const auto facing = static_cast<Facing>(direction);
        for (std::uint8_t distance = 1; distance <= range; ++distance) {
            const auto square = SquareInDirection(position, facing, distance);
            if (!square) {
                break;
            }
            options.emplace_back(*square, facing);
        }
    }
    return options;
}

Board::SquareMask Board::GetBombardment(Team team) const {
    SquareMask covered{};
    for (std::uint8_t row = 0; row < Size; ++row) {
        for (std::uint8_t column = 0; column < Size; ++column) {
            const auto &piece = pieces[row][column];
            if (!piece || piece->team != team || piece->GetFireRange() == 0) {
                continue;
            }

            for (const auto &[square, facing] : GetFireOptions({column, row})) {
                if (facing == piece->facing) {
                    covered[square.row][square.column] = true;
                }
            }
        }
    }
    return covered;
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
            writer.WriteUint8(piece ? static_cast<std::uint8_t>(piece->facing)
                                    : EmptySquare);
            writer.WriteUint8(piece ? static_cast<std::uint8_t>(piece->action)
                                    : EmptySquare);
        }
    }
    // Reserve bytes follow the squares, ordered by Team, then PieceType.
    for (const auto &reserve : reserves) {
        for (const auto count : reserve) {
            writer.WriteUint8(count);
        }
    }
    writer.WriteUint32(turnNumber);
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
            const auto facing = reader.ReadUint8();
            const auto action = reader.ReadUint8();
            if (type == EmptySquare) {
                if (team != EmptySquare || facing != EmptySquare ||
                    action != EmptySquare) {
                    throw std::invalid_argument(
                        "An empty GHQ square cannot contain piece data.");
                }
            } else {
                const Piece piece{static_cast<PieceType>(type),
                                  static_cast<Team>(team),
                                  static_cast<Facing>(facing),
                                  static_cast<ActionState>(action)};
                ValidatePiece(piece);
                board.pieces[row][column] = piece;
            }
        }
    }
    for (auto &reserve : board.reserves) {
        for (auto &count : reserve) {
            count = reader.ReadUint8();
        }
    }

    board.turnNumber = reader.ReadUint32();
    if (board.turnNumber == 0) {
        throw std::invalid_argument("GHQ turn numbers start at one.");
    }

    // Read all facings before resolving pairs so the order of the squares
    // in the snapshot cannot break an existing engagement.
    board.UpdateEngagements();
    return board;
}

const std::optional<Piece> &Board::GetPiece(BoardCoordinate position) const {
    if (position.column >= Size || position.row >= Size) {
        throw std::out_of_range(
            "GHQ board coordinates must be between 0 and 7.");
    }
    return pieces[position.row][position.column];
}

std::optional<Board::BoardCoordinate>
Board::GetEngagedOpponent(BoardCoordinate position) const {
    const auto &piece = GetPiece(position);
    if (!piece || !piece->IsInfantry() ||
        !std::ranges::contains(InfantryFacings, piece->facing)) {
        return std::nullopt;
    }

    const auto square = SquareInDirection(position, piece->facing);
    if (square) {
        const auto &opponent = GetPiece(*square);
        if (opponent && opponent->IsInfantry() &&
            opponent->team != piece->team &&
            opponent->facing == OppositeFacing(piece->facing)) {
            return square;
        }
    }

    return std::nullopt;
}

bool Board::HasAdjacentInfantry(BoardCoordinate position, Team team) const {
    for (const auto facing : InfantryFacings) {
        const auto neighbor = SquareInDirection(position, facing);
        if (neighbor) {
            const auto &piece = GetPiece(*neighbor);
            if (piece && piece->team == team && piece->IsInfantry()) {
                return true;
            }
        }
    }
    return false;
}

void Board::UpdateEngagements() {
    // Infantry already facing each other form the starting pairs. Only
    // unpaired Red infantry need to look for a partner.
    for (std::uint8_t row = 0; row < Size; ++row) {
        for (std::uint8_t column = 0; column < Size; ++column) {
            const BoardCoordinate red{column, row};
            const auto &piece = GetPiece(red);
            if (piece && piece->team == Team::Red && piece->IsInfantry() &&
                !GetEngagedOpponent(red)) {
                SquareMask visited{};
                TryEngage(red, visited);
            }
        }
    }
}

bool Board::TryEngage(BoardCoordinate red, SquareMask &visited) {
    for (const auto facing : InfantryFacings) {
        const auto blue = SquareInDirection(red, facing);
        if (!blue || visited[blue->row][blue->column]) {
            continue;
        }

        auto &opponent = pieces[blue->row][blue->column];
        if (!opponent || opponent->team != Team::Blue ||
            !opponent->IsInfantry()) {
            continue;
        }

        visited[blue->row][blue->column] = true;
        const auto partner = GetEngagedOpponent(*blue);
        // A paired Blue infantry is available if its Red partner can engage
        // someone else. Turn the pieces only after finding a free partner.
        if (partner && !TryEngage(*partner, visited)) {
            continue;
        }

        pieces[red.row][red.column]->facing = facing;
        opponent->facing = OppositeFacing(facing);
        return true;
    }
    return false;
}

std::uint8_t Board::GetReserveCount(Piece piece) const {
    return reserves.at(static_cast<std::size_t>(piece.team))
        .at(static_cast<std::size_t>(piece.type));
}

void Board::SetReserveCount(Piece piece, std::uint8_t count) {
    if (!initialized) {
        throw std::logic_error(
            "Initialize the GHQ board before changing reserves.");
    }
    reserves.at(static_cast<std::size_t>(piece.team))
        .at(static_cast<std::size_t>(piece.type)) = count;
}

std::vector<Board::BoardCoordinate>
Board::GetMovementOptions(BoardCoordinate position) const {
    const auto &piece = GetPiece(position);
    if (!piece || piece->action != ActionState::Ready ||
        !HasActionsRemaining(piece->team)) {
        return {};
    }

    const auto rules = piece->GetMovementRules();
    const auto enemy = piece->team == Team::Red ? Team::Blue : Team::Red;
    const auto enemyFire = GetBombardment(enemy);
    const bool engaged = GetEngagedOpponent(position).has_value();
    std::vector<BoardCoordinate> destinations;
    const auto backRow = piece->team == Team::Red ? 0 : Size - 1;
    if (rules.canParadrop && position.row == backRow) {
        // A paradrop has no intervening squares to check. Nearby empty
        // squares are included here too, so they are only reported once.
        for (std::uint8_t row = 0; row < Size; ++row) {
            for (std::uint8_t column = 0; column < Size; ++column) {
                const BoardCoordinate landing{column, row};
                if (!GetPiece(landing) && !enemyFire[row][column] &&
                    !(engaged && HasAdjacentInfantry(landing, enemy))) {
                    destinations.push_back(landing);
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
                if (GetPiece(destination) ||
                    enemyFire[destination.row][destination.column]) {
                    break;
                }
                // The first step must clear even enemy infantry already in
                // another engagement. Armored infantry may engage on step two.
                if (step == 0 && engaged &&
                    HasAdjacentInfantry(destination, enemy)) {
                    break;
                }
                destinations.push_back(destination);
            }
        }
    }
    return destinations;
}

} // namespace ghqcxx
