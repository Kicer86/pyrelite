
#include "game.h"

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <queue>
#include <set>
#include <stdexcept>
#include <utility>

#include "arena.h"
#include "bounded_terrain.h"
#include "movement.h"
#include "world.h"

namespace pyrelite
{
    namespace
    {
        constexpr int kBombFuseMs = 2000;
        constexpr int kExplosionMs = 400;

        // Movement is expressed in sub-units per millisecond, so a frame moves
        // movementUnits = speed * deltaMs (integer, deterministic). At kSubcell =
        // 1024 the base speed crosses a tile in ~341 ms (~2.9 tiles/s); each Speed
        // power-up adds one sub-unit/ms. Tunable balance knobs.
        constexpr int kBaseSpeedUnitsPerMs = 3;
        constexpr int kSpeedStepUnitsPerMs = 1;

        // How many enemies a generated arena seeds, and how far (Manhattan tiles)
        // they must spawn from the player pocket so the opening is never a death trap.
        constexpr int kEnemyCount = 5;
        constexpr int kEnemySpawnMinDistance = 4;

        // The streamed world seeds its starter enemies within this many tiles of the
        // origin spawn pocket, so the player meets foes at once without scanning the
        // (infinite) world. Per-chunk spawning across the whole world comes later.
        constexpr int kStreamSpawnAreaTiles = 24;

        // Of those, the archetype quota: a greedy Chaser, a ricocheting Bouncer, a
        // pathfinding Hunter and a wall-passing Ghost, with the rest left as random
        // Wanderers. One of each plus a roamer keeps the arena varied; tunable knobs.
        constexpr int kChaserCount = 1;
        constexpr int kBouncerCount = 1;
        constexpr int kHunterCount = 1;
        constexpr int kGhostCount = 1;

        // Decorrelate the enemy RNG stream from the power-up one (same seed would
        // otherwise tie drops to spawns); golden-ratio offset, splitmix64-friendly.
        constexpr std::uint64_t kEnemySeedOffset = 0x9E3779B97F4A7C15ULL;

        // And the perk-offer stream from both of the above, so which perks a level-up
        // shows is decorrelated from spawns and drops yet still seed-deterministic.
        constexpr std::uint64_t kPerkSeedOffset = 0xD1B54A32D192ED03ULL;

        // In-run progression knobs (M3). XP comes from kills only — clearing bricks is
        // already rewarded with power-ups, and a kill-only source means every level-up
        // has a fallen enemy to drop its reward from. Advancing a level costs kXpBase,
        // rising kXpStep each level (L1->2 = base, L2->3 = base + step, ...). A level-up
        // drops a cluster of kPerkChoiceCount perks. All tunable balance.
        constexpr int kXpPerKill = 10;
        constexpr int kXpBase = 10;
        constexpr int kXpStep = 5;
        constexpr int kPerkChoiceCount = 3;

        // Every perk a level-up can drop, in one place (like kPowerUpTypes). A cluster
        // is a distinct subset of size kPerkChoiceCount; today the catalog holds exactly
        // that many, so all appear — adding a perk here (and to PerkType) turns the drop
        // into a real random subset with no other change.
        constexpr PerkType kPerkCatalog[] = {
            PerkType::ExtraBomb,
            PerkType::BiggerBlast,
            PerkType::SwiftFeet,
        };

        // Every kind a brick can drop, in one place. randomPowerUpType draws
        // uniformly from this list, so a new power-up is added here (and to the
        // PowerUpType enum) with no magic count or parallel switch to keep in sync.
        constexpr PowerUpType kPowerUpTypes[] = {
            PowerUpType::BombLimit,
            PowerUpType::BombRange,
            PowerUpType::Speed,
        };

        PowerUpType randomPowerUpType(Rng &rng)
        {
            return kPowerUpTypes[rng.below(
                static_cast<std::uint32_t>(std::size(kPowerUpTypes)))];
        }

        // The archetype for the placed-th spawned enemy. The quota is filled in a
        // fixed order — Chaser, Bouncer, Hunter, Ghost — and any slots beyond it roam
        // as Wanderers. The cutoffs are the running totals of the quota.
        EnemyType enemyTypeForSlot(int placed)
        {
            const int chaserCutoff = kChaserCount;
            const int bouncerCutoff = chaserCutoff + kBouncerCount;
            const int hunterCutoff = bouncerCutoff + kHunterCount;
            const int ghostCutoff = hunterCutoff + kGhostCount;

            if (placed < chaserCutoff)
                return EnemyType::Chaser;
            if (placed < bouncerCutoff)
                return EnemyType::Bouncer;
            if (placed < hunterCutoff)
                return EnemyType::Hunter;
            if (placed < ghostCutoff)
                return EnemyType::Ghost;
            return EnemyType::Wanderer;
        }
    }

    Game::Game(Grid grid)
        : Game(std::move(grid), 1)
    {
    }

    Game::Game(Grid grid, std::uint64_t seed)
        : Game(std::move(grid), seed, Objective::ClearEnemies)
    {
    }

    Game::Game(Grid grid, std::uint64_t seed, Objective objective)
        : m_columns(grid.width())
        , m_rows(grid.height())
        , m_terrain(std::make_unique<BoundedTerrain>(std::move(grid)))
        , m_player(1, 1)
        , m_powerUpRng(seed)
        , m_enemyRng(seed + kEnemySeedOffset)
        , m_perkRng(seed + kPerkSeedOffset)
        , m_objective(objective)
    {
        if (!m_terrain->inBounds(1, 1) || m_terrain->at(1, 1) != Tile::Empty)
            throw std::invalid_argument("Spawn cell (1,1) must be in-bounds and empty");
    }

    Game::Game(int width, int height, std::uint64_t seed)
        : Game(generateArena(width, height, seed), seed)
    {
        spawnEnemies(kEnemyCount);
    }

    Game::Game(std::uint64_t seed, Streamed)
        : m_columns(0)
        , m_rows(0)
        , m_terrain(std::make_unique<World>(seed))
        , m_player(1, 1)
        , m_powerUpRng(seed)
        , m_enemyRng(seed + kEnemySeedOffset)
        , m_perkRng(seed + kPerkSeedOffset)
        , m_objective(Objective::Endless)
    {
        // Materialize the spawn window, then seed a starter set of enemies near the
        // origin pocket (which generateChunk guarantees is open at (1, 1)).
        m_terrain->stream(playerX(), playerY());
        spawnEnemiesIn(0, 0, kStreamSpawnAreaTiles, kStreamSpawnAreaTiles, kEnemyCount);
    }

    Tile Game::tileAt(int x, int y) const
    {
        return m_terrain->at(x, y);
    }

    bool Game::hasBombAt(int x, int y) const
    {
        for (const Bomb &bomb : m_bombs)
        {
            if (bomb.x == x && bomb.y == y)
                return true;
        }
        return false;
    }

    bool Game::hasExplosionAt(int x, int y) const
    {
        for (const Explosion &flame : m_explosions)
        {
            if (flame.x == x && flame.y == y)
                return true;
        }
        return false;
    }

    bool Game::hasPowerUpAt(int x, int y) const
    {
        for (const PowerUp &powerUp : m_powerUps)
        {
            if (powerUp.x == x && powerUp.y == y)
                return true;
        }
        return false;
    }

    bool Game::hasEnemyAt(int x, int y) const
    {
        for (const auto &enemy : m_enemies)
        {
            if (enemy->tileX() == x && enemy->tileY() == y)
                return true;
        }
        return false;
    }

    bool Game::walkable(int x, int y) const
    {
        return m_terrain->simulationActiveAt(x, y)
            && m_terrain->at(x, y) == Tile::Empty
            && !hasBombAt(x, y);
    }

    bool Game::walkableThroughBricks(int x, int y) const
    {
        return m_terrain->simulationActiveAt(x, y)
            && m_terrain->at(x, y) != Tile::Wall
            && !hasBombAt(x, y);
    }

    bool Game::addEnemy(int tileX, int tileY, EnemyType type)
    {
        if (!m_terrain->inBounds(tileX, tileY) || m_terrain->at(tileX, tileY) != Tile::Empty)
            return false;

        m_enemies.push_back(makeEnemy(type, tileX, tileY));
        return true;
    }

    void Game::setVisibleArea(int minX, int minY, int maxX, int maxY)
    {
        m_terrain->setVisibleArea(minX, minY, maxX, maxY);
    }

    // Deterministically seed up to count enemies on empty tiles a safe distance from
    // the player pocket. Candidates are gathered in row-major order, then drawn (and
    // removed) with the enemy RNG, so the same seed always yields the same set. The
    // placement order fills the archetype quota — Chasers, then Bouncers, then Hunters,
    // then Ghosts — and the rest roam as Wanderers.
    void Game::spawnEnemies(int count)
    {
        spawnEnemiesIn(0, 0, m_columns, m_rows, count);
    }

    void Game::spawnEnemiesIn(int minX, int minY, int maxX, int maxY, int count)
    {
        const auto hasEmptyNeighbour = [this](int x, int y)
        {
            return (m_terrain->inBounds(x - 1, y) && m_terrain->at(x - 1, y) == Tile::Empty)
                || (m_terrain->inBounds(x + 1, y) && m_terrain->at(x + 1, y) == Tile::Empty)
                || (m_terrain->inBounds(x, y - 1) && m_terrain->at(x, y - 1) == Tile::Empty)
                || (m_terrain->inBounds(x, y + 1) && m_terrain->at(x, y + 1) == Tile::Empty);
        };

        std::vector<std::pair<int, int>> candidates;
        for (int y = minY; y < maxY; ++y)
        {
            for (int x = minX; x < maxX; ++x)
            {
                if (m_terrain->at(x, y) != Tile::Empty)
                    continue;
                if (std::abs(x - 1) + std::abs(y - 1) < kEnemySpawnMinDistance)
                    continue;
                // Skip pockets walled in by bricks/pillars: an enemy there could
                // never roam, so it would just sit frozen until the player digs it out.
                if (!hasEmptyNeighbour(x, y))
                    continue;
                candidates.emplace_back(x, y);
            }
        }

        for (int placed = 0; placed < count && !candidates.empty(); ++placed)
        {
            const std::size_t pick = m_enemyRng.below(
                static_cast<std::uint32_t>(candidates.size()));
            const auto [x, y] = candidates[pick];
            addEnemy(x, y, enemyTypeForSlot(placed));
            candidates[pick] = candidates.back();
            candidates.pop_back();
        }
    }

    bool Game::tryMove(Direction dir)
    {
        int tx = playerX();
        int ty = playerY();
        stepTile(dir, tx, ty);

        if (!walkable(tx, ty))
            return false;

        m_player.snapTo(tx, ty);
        m_terrain->stream(playerX(), playerY());
        collectPowerUpAtPlayer();
        collectPerkCrystalAtPlayer();
        return true;
    }

    void Game::setMoveDirection(std::optional<Direction> dir)
    {
        m_heldDir = dir;
    }

    bool Game::placeBomb()
    {
        if (static_cast<int>(m_bombs.size()) >= m_bombLimit)
            return false;

        const int tx = playerX();
        const int ty = playerY();
        if (hasBombAt(tx, ty))
            return false;

        m_bombs.push_back(Bomb{tx, ty, kBombFuseMs, m_bombRange});
        return true;
    }

    void Game::queueBomb()
    {
        m_pendingBomb = true;
    }

    bool Game::drainBomb()
    {
        if (!m_pendingBomb)
            return false;
        m_pendingBomb = false;
        return placeBomb();
    }

    int Game::movementUnits(int deltaMs) const
    {
        const int unitsPerMs = kBaseSpeedUnitsPerMs
            + (m_playerSpeed - 1) * kSpeedStepUnitsPerMs;
        return unitsPerMs * deltaMs;
    }

    // Grid-locked continuous movement: while a step is in progress (the player is
    // off-centre) finish it toward the target centre, ignoring input — so a key
    // released mid-tile still completes the step. Only when centred do we sample the
    // held direction and, if the next tile is walkable, commit to the next step.
    bool Game::integrateMovement(int deltaMs)
    {
        if (m_player.centred())
        {
            if (!m_heldDir)
                return false;

            int tx = m_player.tileX();
            int ty = m_player.tileY();
            stepTile(*m_heldDir, tx, ty);
            if (!walkable(tx, ty))
                return false; // blocked against a wall/brick/bomb; stay centred

            m_player.aimAt(tx, ty);
        }

        if (!m_player.advance(movementUnits(deltaMs)))
            return false;

        collectPowerUpAtPlayer();
        collectPerkCrystalAtPlayer();
        return true;
    }

    // Settle the consequences of this tick's positions and flames. Enemies standing
    // in a flame die first; then the player loses if caught in a flame (including
    // their own blast) or sharing a tile with a surviving enemy. Clearing the last
    // enemy wins. Returns true if the outcome changed (a death or a state change).
    bool Game::resolveDeaths()
    {
        // Remember a tile where an enemy fell, so a resulting level-up can drop its
        // perk cluster there (the kill the player just made, made visible as loot).
        std::optional<std::pair<int, int>> killTile;
        for (const auto &enemy : m_enemies)
        {
            if (hasExplosionAt(enemy->tileX(), enemy->tileY()))
            {
                killTile = std::pair{enemy->tileX(), enemy->tileY()};
                break;
            }
        }

        const std::size_t before = m_enemies.size();
        std::erase_if(m_enemies,
            [this](const std::unique_ptr<IEnemy> &enemy)
            {
                return hasExplosionAt(enemy->tileX(), enemy->tileY());
            });
        const std::size_t killed = before - m_enemies.size();
        if (killed > 0)
        {
            awardXp(static_cast<int>(killed) * kXpPerKill);
            m_lastKillTile = killTile;
        }
        const bool killedEnemy = killed > 0;

        const int px = playerX();
        const int py = playerY();
        if (hasExplosionAt(px, py) || hasEnemyAt(px, py))
        {
            m_state = GameState::Lost;
            return true;
        }

        if (m_objective == Objective::ClearEnemies && killedEnemy && m_enemies.empty())
        {
            m_state = GameState::Won;
            return true;
        }

        return killedEnemy;
    }

    void Game::addExplosion(int x, int y)
    {
        m_explosions.push_back(Explosion{x, y, kExplosionMs});
    }

    void Game::dropPowerUp(int x, int y)
    {
        m_powerUps.push_back(PowerUp{x, y, randomPowerUpType(m_powerUpRng)});
    }

    void Game::applyPowerUp(PowerUpType type)
    {
        switch (type)
        {
        case PowerUpType::BombLimit:
            ++m_bombLimit;
            break;
        case PowerUpType::BombRange:
            ++m_bombRange;
            break;
        case PowerUpType::Speed:
            ++m_playerSpeed;
            break;
        }
    }

    void Game::collectPowerUpAtPlayer()
    {
        const int tx = playerX();
        const int ty = playerY();
        const auto it = std::find_if(m_powerUps.begin(), m_powerUps.end(),
            [tx, ty](const PowerUp &powerUp)
            {
                return powerUp.x == tx && powerUp.y == ty;
            });
        if (it != m_powerUps.end())
        {
            applyPowerUp(it->type);
            m_powerUps.erase(it);
        }
    }

    void Game::explode(const Bomb &bomb)
    {
        addExplosion(bomb.x, bomb.y);

        constexpr int dx[4] = {0, 0, -1, 1};
        constexpr int dy[4] = {-1, 1, 0, 0};
        for (int d = 0; d < 4; ++d)
        {
            for (int step = 1; step <= bomb.range; ++step)
            {
                const int x = bomb.x + dx[d] * step;
                const int y = bomb.y + dy[d] * step;
                if (!m_terrain->inBounds(x, y) || m_terrain->at(x, y) == Tile::Wall)
                    break;

                addExplosion(x, y);

                // Chain: any bomb caught in the blast detonates next.
                for (Bomb &other : m_bombs)
                {
                    if (other.x == x && other.y == y)
                        other.fuseMs = 0;
                }

                if (m_terrain->at(x, y) == Tile::Brick)
                {
                    m_terrain->set(x, y, Tile::Empty);
                    dropPowerUp(x, y);
                    break;
                }
            }
        }
    }

    // Both stats are floored at 1 by design: the player can always place a bomb,
    // and a blast always reaches its neighbouring cells. A future "curse" perk
    // that lowers these must respect that floor rather than reach 0.
    void Game::setBombLimit(int limit)
    {
        m_bombLimit = std::max(1, limit);
    }

    void Game::setBombRange(int range)
    {
        m_bombRange = std::max(1, range);
    }

    int Game::xpToNextLevel() const
    {
        return kXpBase + (m_level - 1) * kXpStep;
    }

    void Game::awardXp(int amount)
    {
        m_xp += amount;
    }

    // After the simulation settles, spend one banked level: if enough XP is in hand,
    // the run is live, and no cluster is still waiting to be claimed, level up and drop
    // a fresh perk cluster. Only one cluster is out at a time, so a multi-kill tick that
    // banks several levels pays out one claim after another. Won/Lost take priority (a
    // clearing kill ends the arena rather than dropping loot). Returns true on a level.
    bool Game::checkLevelUp()
    {
        if (m_state != GameState::Playing)
            return false;
        if (!m_perkCrystals.empty())
            return false;
        if (m_xp < xpToNextLevel())
            return false;

        m_xp -= xpToNextLevel();
        ++m_level;

        // Drop where the levelling kill fell (consumed once); a further banked level,
        // claimed later with no fresh kill, clusters around the player instead.
        const auto [ox, oy] = m_lastKillTile.value_or(
            std::pair{playerX(), playerY()});
        m_lastKillTile.reset();
        dropPerkCluster(ox, oy);
        return true;
    }

    // Lay a cluster of kPerkChoiceCount distinct perks as floor pickups, spreading out
    // from the origin over open tiles (BFS; walls block; the player's own tile and any
    // occupied tile are skipped) so even a cramped kill scatters them somewhere
    // reachable. Fewer open tiles than perks (a boxed-in kill) simply drops fewer.
    void Game::dropPerkCluster(int originX, int originY)
    {
        // Draw the distinct perks, ordered canonically for stable presentation.
        std::vector<PerkType> pool(std::begin(kPerkCatalog), std::end(kPerkCatalog));
        const int wanted = std::min<int>(kPerkChoiceCount, static_cast<int>(pool.size()));
        std::vector<PerkType> perks;
        for (int i = 0; i < wanted; ++i)
        {
            const std::size_t pick = m_perkRng.below(
                static_cast<std::uint32_t>(pool.size()));
            perks.push_back(pool[pick]);
            pool[pick] = pool.back();
            pool.pop_back();
        }
        std::sort(perks.begin(), perks.end());

        // Gather up to perks.size() landing tiles, nearest first, by BFS over open floor.
        const auto canLand = [this](int x, int y)
        {
            return m_terrain->inBounds(x, y) && m_terrain->at(x, y) == Tile::Empty
                && !(x == playerX() && y == playerY())
                && !hasBombAt(x, y) && !hasPowerUpAt(x, y);
        };

        constexpr int dx[4] = {0, 0, -1, 1};
        constexpr int dy[4] = {-1, 1, 0, 0};
        std::vector<std::pair<int, int>> tiles;
        std::set<std::pair<int, int>> seen{{originX, originY}};
        std::queue<std::pair<int, int>> frontier;
        frontier.push({originX, originY});
        while (!frontier.empty() && tiles.size() < perks.size())
        {
            const auto [x, y] = frontier.front();
            frontier.pop();
            if (canLand(x, y))
                tiles.emplace_back(x, y);
            for (int d = 0; d < 4; ++d)
            {
                const int nx = x + dx[d];
                const int ny = y + dy[d];
                if (m_terrain->inBounds(nx, ny) && m_terrain->at(nx, ny) == Tile::Empty
                    && seen.insert({nx, ny}).second)
                    frontier.push({nx, ny});
            }
        }

        m_perkCrystals.clear();
        const std::size_t count = std::min(tiles.size(), perks.size());
        for (std::size_t i = 0; i < count; ++i)
            m_perkCrystals.push_back(
                PerkCrystal{tiles[i].first, tiles[i].second, perks[i]});
    }

    void Game::collectPerkCrystalAtPlayer()
    {
        if (m_perkCrystals.empty())
            return;

        const int tx = playerX();
        const int ty = playerY();
        const auto it = std::find_if(m_perkCrystals.begin(), m_perkCrystals.end(),
            [tx, ty](const PerkCrystal &crystal)
            {
                return crystal.x == tx && crystal.y == ty;
            });
        if (it == m_perkCrystals.end())
            return;

        applyPerk(it->type);
        m_perkCrystals.clear(); // claiming one dismisses the rest of the cluster
    }

    void Game::applyPerk(PerkType perk)
    {
        switch (perk)
        {
        case PerkType::ExtraBomb:
            setBombLimit(m_bombLimit + 1);
            break;
        case PerkType::BiggerBlast:
            setBombRange(m_bombRange + 1);
            break;
        case PerkType::SwiftFeet:
            ++m_playerSpeed;
            break;
        }
    }

    bool Game::update(int deltaMs)
    {
        if (deltaMs <= 0)
            return false;
        if (m_state != GameState::Playing)
            return false; // frozen once the run has ended

        // Keep the streamed world resident around the player (a no-op for a bounded
        // arena). Done before movement so this tick's reads hit loaded chunks.
        m_terrain->stream(playerX(), playerY());

        bool changed = drainBomb();
        if (integrateMovement(deltaMs))
            changed = true;

        for (auto &enemy : m_enemies)
        {
            if (!m_terrain->simulationActiveAt(enemy->tileX(), enemy->tileY()))
                continue;
            if (enemy->integrate(*this, m_enemyRng, deltaMs))
                changed = true;
        }

        // Age flames.
        for (Explosion &flame : m_explosions)
            flame.lifeMs -= deltaMs;
        const auto gone = std::remove_if(m_explosions.begin(), m_explosions.end(),
            [](const Explosion &flame) { return flame.lifeMs <= 0; });
        if (gone != m_explosions.end())
        {
            m_explosions.erase(gone, m_explosions.end());
            changed = true;
        }

        // Age fuses.
        for (Bomb &bomb : m_bombs)
            bomb.fuseMs -= deltaMs;

        // Detonate every elapsed bomb, cascading through chains.
        while (true)
        {
            const auto it = std::find_if(m_bombs.begin(), m_bombs.end(),
                [](const Bomb &bomb) { return bomb.fuseMs <= 0; });
            if (it == m_bombs.end())
                break;

            const Bomb bomb = *it;
            m_bombs.erase(it);
            explode(bomb);
            changed = true;
        }

        if (resolveDeaths())
            changed = true;

        // Bank XP earned this tick into a level-up offer, unless the run just ended.
        if (checkLevelUp())
            changed = true;

        return changed;
    }
} // namespace pyrelite
