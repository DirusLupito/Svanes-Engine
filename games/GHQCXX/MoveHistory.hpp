#pragma once

#include "Board.hpp"

#include <svanes/memory/object_pool.hpp>

#include <vector>

namespace ghqcxx {

/**
 * Stores the board positions the server can restore during the current turn.
 */
class MoveHistory final {
public:
    /**
     * Saves the position before a player action.
     *
     * @param board The board to copy.
     */
    void Save(const Board &board);

    /**
     * Restores the most recently saved position and removes it from history.
     *
     * @param board The board to restore.
     *
     * @throws std::invalid_argument if there are no moves to undo.
     */
    void Undo(Board &board);

    /**
     * Discards the positions saved during the turn.
     */
    void Clear();

private:
    // The storage for saved boards. It must outlive the snapshots below.
    svanes::ObjectPool<Board> pool{Board::ActionsPerTurn};

    // The board before each undoable action, oldest first.
    std::vector<svanes::ObjectPool<Board>::Pointer> snapshots;
};

} // namespace ghqcxx
