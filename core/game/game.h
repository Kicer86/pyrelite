
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "grid/grid.h"
#include "enemies/ienemy.h"
#include "game/igame.h"
#include "terrain/iterrain.h"
#include "grid/movement.h"
#include "rng/rng.h"

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

    // A build-defining ability earned on level-up (M3 progression). Distinct from a
    // PowerUp by kind, not just agency: power-ups are the numeric economy (more bombs,
    // range, speed) found on bricks and applied on contact; perks are the reward for
    // killing enemies, arrive as a choose-one cluster (see PerkCrystal), and change how
    // the player fights rather than handing out bigger numbers.
    enum class PerkType { PierceBlast, Shield, RemoteDetonator };

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
        // Run-completion rule, independent of terrain shape: finite arenas win when
        // their roster is cleared; endless runs continue until death.
        enum class Objective { ClearEnemies, Endless };

        // Build from an existing grid; player starts centred on the spawn tile (1, 1).
        explicit Game(Grid grid);
        Game(Grid grid, std::uint64_t seed);
        // Explicit objective overload also keeps run rules testable on a small grid.
        Game(Grid grid, std::uint64_t seed, Objective objective);

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

        // Terrain tile at (x, y); out-of-world reads as Wall. Bounded games retain
        // their fixed extent internally; the streamed world is unbounded and the view
        // reads a window around the player by global coordinate.
        Tile tileAt(int x, int y) const;

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

        // Chance in [0, 100] that destroying a brick drops a power-up. Tunable balance
        // (power-ups are a treat, not a guarantee) and a test seam: tests force 100 for
        // a deterministic drop.
        int powerUpDropPercent() const { return m_powerUpDropPercent; }
        void setPowerUpDropPercent(int percent);

        // Active perk abilities, granted by claiming perk crystals (see PerkType) and
        // exposed for the HUD and tests. PierceBlast lets a blast tear through bricks to
        // its full range instead of stopping at the first one.
        bool pierceBlast() const { return m_pierceBlast; }
        void setPierceBlast(bool on) { m_pierceBlast = on; }

        // Shield (Second Wind): each charge soaks one otherwise-lethal hit (flame or
        // enemy contact) and grants a brief mercy invulnerability, so one hazard spends
        // exactly one charge. invulnerable() exposes that window for the HUD/feel.
        int shieldCharges() const { return m_shieldCharges; }
        void setShieldCharges(int charges);
        bool invulnerable() const { return m_invulnMs > 0; }

        // Remote Detonator: while held, placed bombs freeze their fuses and wait; the
        // player triggers the blast on demand (queueDetonate). Without the perk the
        // command is inert and bombs tick down on their own as usual.
        bool remoteDetonator() const { return m_remoteDetonator; }
        void setRemoteDetonator(bool on) { m_remoteDetonator = on; }

        bool hasBombAt(int x, int y) const;
        bool hasExplosionAt(int x, int y) const;
        bool hasPowerUpAt(int x, int y) const;
        bool hasEnemyAt(int x, int y) const;

        // How many distinct streamed-world enemies the player has killed. Their spawn
        // cells are remembered for the run so a reloaded zone never resurrects them
        // (anti-farm); exposed for introspection/tests, like World::deltaCount.
        std::size_t killedEnemyCount() const { return m_deadEnemies.size(); }

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

        // Retain the view's inclusive global tile rectangle alongside the player-centred
        // simulation window. A no-op for bounded terrain.
        void setVisibleArea(int minX, int minY, int maxX, int maxY);

        // Discrete one-tile step in dir if the target tile is walkable, snapping the
        // player onto that tile centre. Returns true if the player moved. Handy for
        // tests/teleports; live gameplay uses setMoveDirection instead.
        bool tryMove(Direction dir);

        // Set (or clear, with nullopt) the held movement direction. The player moves
        // grid-locked: it keeps travelling along the held direction and can only turn
        // 90 degrees when centred on a tile. A step in progress otherwise runs to the
        // next centre even if the key is released mid-tile — except an about-face, which
        // reverses the current step at once. Integrated in update().
        void setMoveDirection(std::optional<Direction> dir);

        // Drop a bomb on the player's current tile. Returns true if one was placed
        // (under the bomb limit, and no bomb already on that tile).
        bool placeBomb();

        // Queue a one-shot bomb to be placed at the start of the next update() step,
        // so it is ordered deterministically with the rest of the simulation.
        void queueBomb();

        // Queue a one-shot Remote Detonator trigger, drained at the next update() step:
        // every live bomb is set to blow this tick. Inert without the remoteDetonator perk.
        void queueDetonate();

        // Advance the simulation by deltaMs: drain the queued bomb, move the player,
        // move the enemies, age flames, detonate elapsed bombs (cross blast up to
        // range, stopped by walls, one brick per arm, chain-detonating caught bombs),
        // then settle deaths. A no-op once the game has ended. Returns true if
        // anything visible changed.
        bool update(int deltaMs);

    private:
        // Where a streamed enemy came from: the generation zone that owns it and the
        // pristine floor cell it spawned on. Bounded/test enemies (addEnemy) carry no
        // origin (persistent == false): they are never owned by a zone nor persisted.
        struct EnemyOrigin
        {
            int zoneX;
            int zoneY;
            int spawnX;
            int spawnY;
            bool persistent;
        };

        bool drainBomb();
        bool drainDetonate();
        bool integrateMovement(int deltaMs);
        // The direction of the step currently in progress. Precondition: the player is
        // off-centre (a step is underway); used to detect an about-face request.
        Direction currentStepDirection() const;
        bool resolveDeaths();
        void spawnEnemies(int count);
        void spawnEnemiesIn(int minX, int minY, int maxX, int maxY, int count);
        bool placeEnemy(int tileX, int tileY, EnemyType type, const EnemyOrigin &origin);
        // Spawn/evict whole zone rosters as the player's active window moves over the
        // streamed world; a no-op for bounded games.
        void refreshActiveZones();
        void spawnZoneEnemies(int zoneX, int zoneY);
        void despawnZone(int zoneX, int zoneY);
        int movementUnits(int deltaMs) const;
        void explode(const Bomb &bomb);
        void addExplosion(int x, int y);
        void maybeDropPowerUp(int x, int y);
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
        // Spawn origin per enemy, aligned 1:1 with m_enemies (push and erase in lockstep).
        std::vector<EnemyOrigin> m_enemyOrigins;
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
        Objective m_objective = Objective::ClearEnemies;
        std::optional<Direction> m_heldDir;
        bool m_pendingBomb = false;
        int m_powerUpDropPercent = 30; // default brick power-up drop chance, in percent
        bool m_pierceBlast = false;    // perk: blast tears through bricks to full range
        int m_shieldCharges = 0;       // perk: lethal hits the Shield can still soak
        int m_invulnMs = 0;            // remaining mercy invulnerability after a save
        bool m_remoteDetonator = false; // perk: bombs wait for a manual trigger
        bool m_pendingDetonate = false; // a queued remote trigger, drained in update()

        // Streamed-world enemy lifecycle. Only the streamed game owns zone rosters; the
        // bounded constructors leave m_streamed false so all of this stays inert.
        bool m_streamed = false;
        std::uint64_t m_worldSeed = 0;
        std::set<std::pair<int, int>> m_activeZones;
        std::set<std::pair<int, int>> m_deadEnemies; // global spawn tiles, never resurrected
        std::optional<std::pair<int, int>> m_lastPlayerChunk;
    };
} // namespace pyrelite
