
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "enemy.h"
#include "grid.h"
#include "igame.h"
#include "movement.h"
#include "rng.h"

namespace pyrelite
{
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

    // Playing until the player either clears every enemy (Won) or is caught by a
    // flame or an enemy (Lost). Both end states freeze the simulation.
    enum class GameState { Playing, Won, Lost };

    // Central game state: the arena grid, the player, active bombs, and live
    // explosion flames. Implements IGame so enemy AI sees only the slice it needs.
    // No Qt.
    class Game : public IGame
    {
    public:
        // Build from an existing grid; player starts centred on the spawn tile (1, 1).
        explicit Game(Grid grid);
        Game(Grid grid, std::uint64_t seed);

        // Convenience: generate a deterministic arena, then place the player.
        Game(int width, int height, std::uint64_t seed);

        const Grid &grid() const { return m_grid; }

        GameState state() const { return m_state; }

        // Player position in sub-units (kSubcell per tile) for smooth rendering...
        int playerSubX() const { return m_playerSubX; }
        int playerSubY() const { return m_playerSubY; }
        // ...and as the tile it currently occupies (nearest centre), used for bomb
        // placement, pick-ups and collision.
        int playerX() const override { return (m_playerSubX + kSubcell / 2) / kSubcell; }
        int playerY() const override { return (m_playerSubY + kSubcell / 2) / kSubcell; }

        const std::vector<Bomb> &bombs() const { return m_bombs; }
        const std::vector<Explosion> &explosions() const { return m_explosions; }
        const std::vector<PowerUp> &powerUps() const { return m_powerUps; }
        const std::vector<std::unique_ptr<IEnemy>> &enemies() const { return m_enemies; }

        int bombLimit() const { return m_bombLimit; }
        void setBombLimit(int limit);
        int bombRange() const { return m_bombRange; }
        void setBombRange(int range);
        int playerSpeed() const { return m_playerSpeed; }

        bool hasBombAt(int x, int y) const;
        bool hasExplosionAt(int x, int y) const;
        bool hasPowerUpAt(int x, int y) const;
        bool hasEnemyAt(int x, int y) const;

        // Whether a moving entity may occupy tile (x, y): in-bounds, empty, and not
        // blocked by a live bomb. From IGame, so enemy archetypes can navigate.
        bool walkable(int x, int y) const override;

        // Place an enemy of the given archetype centred on a walkable tile. Returns
        // false (placing nothing) if the tile is out of bounds or not Empty. The real
        // arena spawns enemies deterministically from the seed; this is also the test
        // seam, defaulting to a Wanderer.
        bool addEnemy(int tileX, int tileY, EnemyType type = EnemyType::Wanderer);

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
        // move the enemies, age flames, detonate elapsed bombs (cross blast up to
        // range, stopped by walls, one brick per arm, chain-detonating caught bombs),
        // then settle deaths. A no-op once the game has ended. Returns true if
        // anything visible changed.
        bool update(int deltaMs);

    private:
        bool drainBomb();
        bool integrateMovement(int deltaMs);
        bool resolveDeaths();
        void spawnEnemies(int count);
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
        std::vector<std::unique_ptr<IEnemy>> m_enemies;
        int m_bombLimit = 1;
        int m_bombRange = 2;
        int m_playerSpeed = 1;
        Rng m_powerUpRng;
        Rng m_enemyRng;
        GameState m_state = GameState::Playing;
        std::optional<Direction> m_heldDir;
        bool m_pendingBomb = false;
    };
} // namespace pyrelite
