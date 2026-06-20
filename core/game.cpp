
#include "game.h"

#include <algorithm>
#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <utility>

#include "arena.h"
#include "movement.h"

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
        constexpr int kEnemyCount = 3;
        constexpr int kEnemySpawnMinDistance = 4;

        // Of those, how many are Chasers and how many Bouncers (the rest are
        // Wanderers). One hunter, one ricocheting bouncer and one roamer keeps the
        // arena varied but still dodgeable; tunable balance knobs.
        constexpr int kChaserCount = 1;
        constexpr int kBouncerCount = 1;

        // Decorrelate the enemy RNG stream from the power-up one (same seed would
        // otherwise tie drops to spawns); golden-ratio offset, splitmix64-friendly.
        constexpr std::uint64_t kEnemySeedOffset = 0x9E3779B97F4A7C15ULL;

        PowerUpType randomPowerUpType(Rng &rng)
        {
            switch (rng.below(3))
            {
            case 0:
                return PowerUpType::BombLimit;
            case 1:
                return PowerUpType::BombRange;
            default:
                return PowerUpType::Speed;
            }
        }
    }

    Game::Game(Grid grid)
        : Game(std::move(grid), 1)
    {
    }

    Game::Game(Grid grid, std::uint64_t seed)
        : m_grid(std::move(grid))
        , m_playerSubX(kSubcell)
        , m_playerSubY(kSubcell)
        , m_targetSubX(kSubcell)
        , m_targetSubY(kSubcell)
        , m_powerUpRng(seed)
        , m_enemyRng(seed + kEnemySeedOffset)
    {
        if (!m_grid.inBounds(1, 1) || m_grid.at(1, 1) != Tile::Empty)
            throw std::invalid_argument("Spawn cell (1,1) must be in-bounds and empty");
    }

    Game::Game(int width, int height, std::uint64_t seed)
        : Game(generateArena(width, height, seed), seed)
    {
        spawnEnemies(kEnemyCount);
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
        return m_grid.inBounds(x, y) && m_grid.at(x, y) == Tile::Empty
            && !hasBombAt(x, y);
    }

    bool Game::addEnemy(int tileX, int tileY, EnemyType type)
    {
        if (!m_grid.inBounds(tileX, tileY) || m_grid.at(tileX, tileY) != Tile::Empty)
            return false;

        m_enemies.push_back(makeEnemy(type, tileX, tileY));
        return true;
    }

    // Deterministically seed up to count enemies on empty tiles a safe distance from
    // the player pocket. Candidates are gathered in row-major order, then drawn (and
    // removed) with the enemy RNG, so the same seed always yields the same set. The
    // first kChaserCount placed are Chasers, the next kBouncerCount Bouncers; the rest
    // roam as Wanderers.
    void Game::spawnEnemies(int count)
    {
        const auto hasEmptyNeighbour = [this](int x, int y)
        {
            return (m_grid.inBounds(x - 1, y) && m_grid.at(x - 1, y) == Tile::Empty)
                || (m_grid.inBounds(x + 1, y) && m_grid.at(x + 1, y) == Tile::Empty)
                || (m_grid.inBounds(x, y - 1) && m_grid.at(x, y - 1) == Tile::Empty)
                || (m_grid.inBounds(x, y + 1) && m_grid.at(x, y + 1) == Tile::Empty);
        };

        std::vector<std::pair<int, int>> candidates;
        for (int y = 0; y < m_grid.height(); ++y)
        {
            for (int x = 0; x < m_grid.width(); ++x)
            {
                if (m_grid.at(x, y) != Tile::Empty)
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
            const EnemyType type =
                placed < kChaserCount ? EnemyType::Chaser
                : placed < kChaserCount + kBouncerCount ? EnemyType::Bouncer
                : EnemyType::Wanderer;
            addEnemy(x, y, type);
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

        m_playerSubX = m_targetSubX = tx * kSubcell;
        m_playerSubY = m_targetSubY = ty * kSubcell;
        collectPowerUpAtPlayer();
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
        const bool centred = m_playerSubX == m_targetSubX && m_playerSubY == m_targetSubY;
        if (centred)
        {
            if (!m_heldDir)
                return false;

            int tx = m_playerSubX / kSubcell;
            int ty = m_playerSubY / kSubcell;
            stepTile(*m_heldDir, tx, ty);
            if (!walkable(tx, ty))
                return false; // blocked against a wall/brick/bomb; stay centred

            m_targetSubX = tx * kSubcell;
            m_targetSubY = ty * kSubcell;
        }

        const int v = movementUnits(deltaMs);
        const int beforeX = m_playerSubX;
        const int beforeY = m_playerSubY;
        m_playerSubX = approach(m_playerSubX, m_targetSubX, v);
        m_playerSubY = approach(m_playerSubY, m_targetSubY, v);

        if (m_playerSubX == beforeX && m_playerSubY == beforeY)
            return false;

        collectPowerUpAtPlayer();
        return true;
    }

    // Settle the consequences of this tick's positions and flames. Enemies standing
    // in a flame die first; then the player loses if caught in a flame (including
    // their own blast) or sharing a tile with a surviving enemy. Clearing the last
    // enemy wins. Returns true if the outcome changed (a death or a state change).
    bool Game::resolveDeaths()
    {
        const std::size_t before = m_enemies.size();
        std::erase_if(m_enemies,
            [this](const std::unique_ptr<IEnemy> &enemy)
            {
                return hasExplosionAt(enemy->tileX(), enemy->tileY());
            });
        const bool killedEnemy = m_enemies.size() < before;

        const int px = playerX();
        const int py = playerY();
        if (hasExplosionAt(px, py) || hasEnemyAt(px, py))
        {
            m_state = GameState::Lost;
            return true;
        }

        if (killedEnemy && m_enemies.empty())
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
                if (!m_grid.inBounds(x, y) || m_grid.at(x, y) == Tile::Wall)
                    break;

                addExplosion(x, y);

                // Chain: any bomb caught in the blast detonates next.
                for (Bomb &other : m_bombs)
                {
                    if (other.x == x && other.y == y)
                        other.fuseMs = 0;
                }

                if (m_grid.at(x, y) == Tile::Brick)
                {
                    m_grid.set(x, y, Tile::Empty);
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

    bool Game::update(int deltaMs)
    {
        if (deltaMs <= 0)
            return false;
        if (m_state != GameState::Playing)
            return false; // frozen once the run has ended

        bool changed = drainBomb();
        if (integrateMovement(deltaMs))
            changed = true;

        for (auto &enemy : m_enemies)
        {
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

        return changed;
    }
} // namespace pyrelite
