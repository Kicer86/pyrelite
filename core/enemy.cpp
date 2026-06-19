
#include "enemy.h"

#include "chaser.h"
#include "game.h"
#include "wanderer.h"

namespace pyrelite
{
    namespace
    {
        // Enemies crawl slower than the player's base speed so they stay dodgeable.
        constexpr int kEnemySpeedUnitsPerMs = 2;
    }

    IEnemy::IEnemy(int tileX, int tileY)
        : m_subX(tileX * kSubcell)
        , m_subY(tileY * kSubcell)
        , m_targetSubX(tileX * kSubcell)
        , m_targetSubY(tileY * kSubcell)
    {
    }

    bool IEnemy::integrate(const Game &game, Rng &rng, int deltaMs)
    {
        const bool centred = m_subX == m_targetSubX && m_subY == m_targetSubY;
        if (centred)
        {
            const std::optional<Direction> next = chooseDirection(game, rng);
            if (!next)
                return false; // boxed in

            m_dir = *next;
            int ahead = tileX();
            int aheadY = tileY();
            stepTile(m_dir, ahead, aheadY);
            m_targetSubX = ahead * kSubcell;
            m_targetSubY = aheadY * kSubcell;
        }

        const int v = kEnemySpeedUnitsPerMs * deltaMs;
        const int beforeX = m_subX;
        const int beforeY = m_subY;
        m_subX = approach(m_subX, m_targetSubX, v);
        m_subY = approach(m_subY, m_targetSubY, v);
        return m_subX != beforeX || m_subY != beforeY;
    }

    std::optional<Direction> IEnemy::randomWalkableDir(const Game &game, Rng &rng) const
    {
        const int tx = tileX();
        const int ty = tileY();

        Direction options[4];
        int count = 0;
        for (const Direction d : {Direction::Up, Direction::Down,
                 Direction::Left, Direction::Right})
        {
            int nx = tx;
            int ny = ty;
            stepTile(d, nx, ny);
            if (game.walkable(nx, ny))
                options[count++] = d;
        }
        if (count == 0)
            return std::nullopt;
        return options[rng.below(static_cast<std::uint32_t>(count))];
    }

    std::unique_ptr<IEnemy> makeEnemy(EnemyType type, int tileX, int tileY)
    {
        switch (type)
        {
        case EnemyType::Chaser:
            return std::make_unique<Chaser>(tileX, tileY);
        case EnemyType::Wanderer:
            break;
        }
        return std::make_unique<Wanderer>(tileX, tileY);
    }
} // namespace pyrelite
