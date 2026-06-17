
#pragma once

#include <cstdint>
#include <vector>

#include "grid.h"

namespace pyrelite
{
    enum class Direction { Up, Down, Left, Right };

    struct Bomb
    {
        int x;
        int y;
        int fuseMs;
    };

    // Central game state: the arena grid, the player, and active bombs. Future
    // slices (explosions, enemies, power-ups) extend this aggregate. No Qt.
    class Game
    {
    public:
        // Build from an existing grid; player starts at the spawn corner (1, 1).
        explicit Game(Grid grid);

        // Convenience: generate a deterministic arena, then place the player.
        Game(int width, int height, std::uint64_t seed);

        const Grid &grid() const { return m_grid; }
        int playerX() const { return m_playerX; }
        int playerY() const { return m_playerY; }

        const std::vector<Bomb> &bombs() const { return m_bombs; }
        int bombLimit() const { return m_bombLimit; }
        bool hasBombAt(int x, int y) const;

        // Step the player one cell in dir if the target is walkable. Returns
        // true if the player actually moved.
        bool tryMove(Direction dir);

        // Drop a bomb on the player's cell. Returns true if one was placed
        // (under the bomb limit, and no bomb already on that cell).
        bool placeBomb();

        // Advance bomb fuses by deltaMs. Returns true if any bomb was removed
        // (detonation effects come in a later slice).
        bool update(int deltaMs);

    private:
        bool walkable(int x, int y) const;

        Grid m_grid;
        int m_playerX;
        int m_playerY;
        std::vector<Bomb> m_bombs;
        int m_bombLimit = 1;
    };
} // namespace pyrelite
