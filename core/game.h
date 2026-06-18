
#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "grid.h"
#include "rng.h"

namespace pyrelite
{
    enum class Direction { Up, Down, Left, Right };

    // Sub-cell resolution. Moving entities (the player; later enemies) store their
    // position in these integer sub-units, kSubcell per tile, so movement is
    // continuous yet bit-exact on every platform (no float). That keeps seeded
    // runs reproducible and headless asserts exact. Tile coords are pos / kSubcell.
    inline constexpr int kSubcell = 1024;

    struct Bomb
    {
        int x;
        int y;
        int fuseMs;
        int range;
    };

    struct Explosion
    {
        int x;
        int y;
        int lifeMs;
    };

    enum class PowerUpType { BombLimit, BombRange, Speed };

    struct PowerUp
    {
        int x;
        int y;
        PowerUpType type;
    };

    // Central game state: the arena grid, the player, active bombs, and live
    // explosion flames. Future slices (enemies) extend this. No Qt.
    class Game
    {
    public:
        // Build from an existing grid; player starts centred on the spawn tile (1, 1).
        explicit Game(Grid grid);
        Game(Grid grid, std::uint64_t seed);

        // Convenience: generate a deterministic arena, then place the player.
        Game(int width, int height, std::uint64_t seed);

        const Grid &grid() const { return m_grid; }

        // Player position in sub-units (kSubcell per tile) for smooth rendering...
        int playerSubX() const { return m_playerSubX; }
        int playerSubY() const { return m_playerSubY; }
        // ...and as the tile it currently occupies (nearest centre), used for bomb
        // placement, pick-ups and collision.
        int playerX() const { return (m_playerSubX + kSubcell / 2) / kSubcell; }
        int playerY() const { return (m_playerSubY + kSubcell / 2) / kSubcell; }

        const std::vector<Bomb> &bombs() const { return m_bombs; }
        const std::vector<Explosion> &explosions() const { return m_explosions; }
        const std::vector<PowerUp> &powerUps() const { return m_powerUps; }

        int bombLimit() const { return m_bombLimit; }
        void setBombLimit(int limit);
        int bombRange() const { return m_bombRange; }
        void setBombRange(int range);
        int playerSpeed() const { return m_playerSpeed; }

        bool hasBombAt(int x, int y) const;
        bool hasExplosionAt(int x, int y) const;
        bool hasPowerUpAt(int x, int y) const;

        // Discrete one-tile step in dir if the target tile is walkable, snapping the
        // player onto that tile centre. Returns true if the player moved. Handy for
        // tests/teleports; live gameplay uses setMoveDirection instead.
        bool tryMove(Direction dir);

        // Set (or clear, with nullopt) the held movement direction. The player moves
        // grid-locked: it keeps travelling along the held direction, can only turn
        // when centred on a tile, and always finishes the current step even if the
        // key is released mid-tile. Integrated in update().
        void setMoveDirection(std::optional<Direction> dir);

        // Drop a bomb on the player's current tile. Returns true if one was placed
        // (under the bomb limit, and no bomb already on that tile).
        bool placeBomb();

        // Queue a one-shot bomb to be placed at the start of the next update() step,
        // so it is ordered deterministically with the rest of the simulation.
        void queueBomb();

        // Advance the simulation by deltaMs: drain the queued bomb, move the player,
        // age flames, and detonate elapsed bombs (cross blast up to range, stopped by
        // walls, one brick per arm, chain-detonating caught bombs). Returns true if
        // anything visible changed.
        bool update(int deltaMs);

    private:
        bool walkable(int x, int y) const;
        bool drainBomb();
        bool integrateMovement(int deltaMs);
        int movementUnits(int deltaMs) const;
        void explode(const Bomb &bomb);
        void addExplosion(int x, int y);
        void dropPowerUp(int x, int y);
        void collectPowerUpAtPlayer();
        void applyPowerUp(PowerUpType type);

        Grid m_grid;
        int m_playerSubX;
        int m_playerSubY;
        int m_targetSubX;
        int m_targetSubY;
        std::vector<Bomb> m_bombs;
        std::vector<Explosion> m_explosions;
        std::vector<PowerUp> m_powerUps;
        int m_bombLimit = 1;
        int m_bombRange = 2;
        int m_playerSpeed = 1;
        Rng m_powerUpRng;
        std::optional<Direction> m_heldDir;
        bool m_pendingBomb = false;
    };
} // namespace pyrelite
