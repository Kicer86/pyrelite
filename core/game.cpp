
#include "game.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "arena.h"

namespace pyrelite
{
    namespace
    {
        constexpr int kBombFuseMs = 2000;
        constexpr int kExplosionMs = 400;

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
        , m_playerX(1)
        , m_playerY(1)
        , m_powerUpRng(seed)
    {
        if (!m_grid.inBounds(1, 1) || m_grid.at(1, 1) != Tile::Empty)
            throw std::invalid_argument("Spawn cell (1,1) must be in-bounds and empty");
    }

    Game::Game(int width, int height, std::uint64_t seed)
        : Game(generateArena(width, height, seed), seed)
    {
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

    bool Game::walkable(int x, int y) const
    {
        return m_grid.inBounds(x, y) && m_grid.at(x, y) == Tile::Empty
            && !hasBombAt(x, y);
    }

    bool Game::tryMove(Direction dir)
    {
        int nx = m_playerX;
        int ny = m_playerY;
        switch (dir)
        {
        case Direction::Up:
            --ny;
            break;
        case Direction::Down:
            ++ny;
            break;
        case Direction::Left:
            --nx;
            break;
        case Direction::Right:
            ++nx;
            break;
        }

        if (!walkable(nx, ny))
            return false;

        m_playerX = nx;
        m_playerY = ny;
        collectPowerUpAtPlayer();
        return true;
    }

    bool Game::placeBomb()
    {
        if (static_cast<int>(m_bombs.size()) >= m_bombLimit)
            return false;
        if (hasBombAt(m_playerX, m_playerY))
            return false;

        m_bombs.push_back(Bomb{m_playerX, m_playerY, kBombFuseMs, m_bombRange});
        return true;
    }

    void Game::queueMove(Direction dir)
    {
        m_pendingMove = dir;
    }

    void Game::queueBomb()
    {
        m_pendingBomb = true;
    }

    // Apply queued input at a single deterministic point per step: the bomb (on
    // the current cell) first, then the move (which may step off it). Each intent
    // is one-shot. Returns whether anything visible changed.
    bool Game::drainInput()
    {
        bool changed = false;
        if (m_pendingBomb)
        {
            if (placeBomb())
                changed = true;
            m_pendingBomb = false;
        }
        if (m_pendingMove)
        {
            if (tryMove(*m_pendingMove))
                changed = true;
            m_pendingMove.reset();
        }
        return changed;
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
        const auto it = std::find_if(m_powerUps.begin(), m_powerUps.end(),
            [this](const PowerUp &powerUp)
            {
                return powerUp.x == m_playerX && powerUp.y == m_playerY;
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

        bool changed = drainInput();

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

        return changed;
    }
} // namespace pyrelite
