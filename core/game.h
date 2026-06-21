
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "grid.h"
#include "ienemy.h"
#include "igame.h"
#include "iterrain.h"
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

    // An in-run upgrade earned on level-up (M3 progression). Distinct from a PowerUp:
    // power-ups drop at random from bricks and apply on contact; perks are the reward
    // for killing enemies and arrive as a choose-one cluster (see PerkCrystal). Their
    // effects can overlap today, but the agency differs — a perk you pick, a power-up
    // you happen upon.
    enum class PerkType { ExtraBomb, BiggerBlast, SwiftFeet };

    // One crystal of the cluster dropped on level-up. The player walks onto one to
    // claim its perk; the rest of the cluster then vanishes — a choose-1-of-N made by
    // movement under pressure, with no pause. Static (tile coords), like a PowerUp.
    struct PerkCrystal
    {
        int x;
        int y;
        PerkType type;
    };

    // Playing until the player either clears every enemy (Won) or is caught by a flame
    // or an enemy (Lost). Both end states freeze the simulation. Levelling up never
    // does — the run is not interrupted; the reward drops onto the floor instead.
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

        // Tag selecting the infinite streamed-world constructor (the play mode), vs the
        // bounded Grid constructors above (fixed arenas / headless tests).
        struct Streamed
        {
        };

        // Build on the infinite streamed World: the player spawns at the origin pocket
        // (1, 1), a starter set of enemies seeds nearby, and the run is endless (no
        // "clear all enemies" win — only the World streams under the player each tick).
        Game(std::uint64_t seed, Streamed);

        // Terrain tile at (x, y); out-of-world reads as Wall. For a bounded arena the
        // playable extent is columns() x rows(); the streamed world is unbounded (its
        // view reads a window around the player by global coordinate).
        Tile tileAt(int x, int y) const;
        int columns() const { return m_columns; }
        int rows() const { return m_rows; }

        GameState state() const { return m_state; }

        // In-run progression (M3): experience earned this run, the current level (from
        // 1), and the XP needed to advance from it to the next. Killing an enemy grants
        // XP; crossing the threshold levels up and drops a perk cluster (perkCrystals)
        // for the player to pick from — the run never pauses.
        int xp() const { return m_xp; }
        int level() const { return m_level; }
        int xpToNextLevel() const;

        // The perk cluster currently lying on the floor (empty when none is pending).
        // Walking onto any one of them claims its perk and clears the rest.
        const std::vector<PerkCrystal> &perkCrystals() const { return m_perkCrystals; }

        // Player position in sub-units (kSubcell per tile) for smooth rendering...
        int playerSubX() const { return m_player.subX; }
        int playerSubY() const { return m_player.subY; }
        // ...and as the tile it currently occupies (nearest centre), used for bomb
        // placement, pick-ups and collision.
        int playerX() const override { return m_player.tileX(); }
        int playerY() const override { return m_player.tileY(); }

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

        // As walkable, but bricks count as passable (only solid walls and bombs block).
        // The Ghost archetype moves by this rule.
        bool walkableThroughBricks(int x, int y) const override;

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
        void spawnEnemiesIn(int minX, int minY, int maxX, int maxY, int count);
        int movementUnits(int deltaMs) const;
        void explode(const Bomb &bomb);
        void addExplosion(int x, int y);
        void dropPowerUp(int x, int y);
        void collectPowerUpAtPlayer();
        void applyPowerUp(PowerUpType type);
        void awardXp(int amount);
        bool checkLevelUp();
        void dropPerkCluster(int originX, int originY);
        void collectPerkCrystalAtPlayer();
        void applyPerk(PerkType perk);

        int m_columns;
        int m_rows;
        std::unique_ptr<ITerrain> m_terrain;
        GridMover m_player;
        std::vector<Bomb> m_bombs;
        std::vector<Explosion> m_explosions;
        std::vector<PowerUp> m_powerUps;
        std::vector<std::unique_ptr<IEnemy>> m_enemies;
        int m_bombLimit = 1;
        int m_bombRange = 2;
        int m_playerSpeed = 1;
        int m_xp = 0;
        int m_level = 1;
        std::vector<PerkCrystal> m_perkCrystals;
        std::optional<std::pair<int, int>> m_lastKillTile;
        Rng m_powerUpRng;
        Rng m_enemyRng;
        Rng m_perkRng;
        GameState m_state = GameState::Playing;
        // Whether clearing every enemy wins. True for a bounded arena (a finite roster);
        // false for the endless streamed world (enemies are open-ended, so there is no
        // "all" to clear) — the run then ends only on death.
        bool m_winnable = true;
        std::optional<Direction> m_heldDir;
        bool m_pendingBomb = false;
    };
} // namespace pyrelite
