
#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "grid.h"
#include "rng.h"

namespace pyrelite
{
    enum class Direction { Up, Down, Left, Right };

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
        // Build from an existing grid; player starts at the spawn corner (1, 1).
        explicit Game(Grid grid);
        Game(Grid grid, std::uint64_t seed);

        // Convenience: generate a deterministic arena, then place the player.
        Game(int width, int height, std::uint64_t seed);

        const Grid &grid() const { return m_grid; }
        int playerX() const { return m_playerX; }
        int playerY() const { return m_playerY; }

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

        // Step the player one cell in dir if the target is walkable. Returns
        // true if the player actually moved.
        bool tryMove(Direction dir);

        // Drop a bomb on the player's cell. Returns true if one was placed
        // (under the bomb limit, and no bomb already on that cell).
        bool placeBomb();

        // Queue an input intent to be applied at the start of the next update()
        // step, so input resolves in a deterministic order with the rest of the
        // simulation instead of mutating state directly between frames. The
        // latest queued move wins; a queued bomb is placed before the move, so a
        // "drop and run" leaves the bomb on the cell the player steps off.
        void queueMove(Direction dir);
        void queueBomb();

        // Advance fuses and flames by deltaMs. A bomb whose fuse elapses explodes
        // in a cross up to its range, stopped by walls, destroying one brick per
        // arm and chain-detonating bombs caught in the blast. Returns true if
        // anything visible changed (bombs, flames, or destroyed bricks).
        bool update(int deltaMs);

    private:
        bool walkable(int x, int y) const;
        void explode(const Bomb &bomb);
        void addExplosion(int x, int y);
        void dropPowerUp(int x, int y);
        void collectPowerUpAtPlayer();
        void applyPowerUp(PowerUpType type);
        bool drainInput();

        Grid m_grid;
        int m_playerX;
        int m_playerY;
        std::vector<Bomb> m_bombs;
        std::vector<Explosion> m_explosions;
        std::vector<PowerUp> m_powerUps;
        int m_bombLimit = 1;
        int m_bombRange = 2;
        int m_playerSpeed = 1;
        Rng m_powerUpRng;
        std::optional<Direction> m_pendingMove;
        bool m_pendingBomb = false;
    };
} // namespace pyrelite
