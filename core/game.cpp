
#include "game.h"

#include <algorithm>
#include <utility>

#include "arena.h"

namespace pyrelite
{
    namespace
    {
        constexpr int kBombFuseMs = 2000;
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

        m_bombs.push_back(Bomb{m_playerX, m_playerY, kBombFuseMs});
        return true;
    }

    bool Game::update(int deltaMs)
    {
        for (Bomb &bomb : m_bombs)
            bomb.fuseMs -= deltaMs;

        const auto detonated = std::remove_if(m_bombs.begin(), m_bombs.end(),
            [](const Bomb &bomb) { return bomb.fuseMs <= 0; });
        if (detonated == m_bombs.end())
            return false;

        m_bombs.erase(detonated, m_bombs.end());
        return true;
    }
} // namespace pyrelite
