#include "MoveHistory.hpp"

#include <stdexcept>

namespace ghqcxx {

void MoveHistory::Save(const Board &board) {
    snapshots.push_back(pool.Make(board));
}

void MoveHistory::Undo(Board &board) {
    if (snapshots.empty()) {
        throw std::invalid_argument("There are no moves to undo this turn.");
    }
    board = *snapshots.back();
    snapshots.pop_back();
}

void MoveHistory::Clear() { snapshots.clear(); }

} // namespace ghqcxx
