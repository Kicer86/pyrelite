
#include "game.h"

#include <algorithm>
#include <utility>

#include "arena.h"

namespace pyrelite
{
    namespace
    {
        constexpr int kBombFuseMs = 2000;
        constexpr int kExplosionMs = 400;
    }

    Game::Game(Grid grid)
        : m_grid(std::move(grid))
        , m_playerX(1)
        , m_playerY(1)
    {
    }

    Game::Game(int width, int height, std::uint64_t seed)
        : Game(generateArena(width, height, seed))
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

    void Game::addExplosion(int x, int y)
    {
        m_explosions.push_back(Explosion{x, y, kExplosionMs});
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
                    break;
                }
            }
        }
    }

    bool Game::update(int deltaMs)
    {
        bool changed = false;

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
