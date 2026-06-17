
#pragma once

#include <cstdint>

#include "grid.h"

namespace pyrelite {

enum class Direction { Up, Down, Left, Right };

// Central game state: the arena grid plus the player. Future slices (bombs,
// enemies, power-ups) extend this aggregate. Pure logic, no Qt.
class Game {
public:
    // Build from an existing grid; player starts at the spawn corner (1, 1).
    explicit Game(Grid grid);

    // Convenience: generate a deterministic arena, then place the player.
    Game(int width, int height, std::uint64_t seed);

    const Grid &grid() const { return grid_; }
    int playerX() const { return playerX_; }
    int playerY() const { return playerY_; }

    // Step the player one cell in `dir` if the target is walkable.
    // Returns true if the player actually moved.
    bool tryMove(Direction dir);

private:
    bool walkable(int x, int y) const;

    Grid grid_;
    int playerX_;
    int playerY_;
};

} // namespace pyrelite
