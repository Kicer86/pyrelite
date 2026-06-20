
#include "enemy.h"

#include "bouncer.h"
#include "chaser.h"
#include "igame.h"
#include "irng.h"
#include "wanderer.h"

namespace pyrelite
{
    namespace
    {
        // Enemies crawl slower than the player's base speed so they stay dodgeable.
        constexpr int kEnemySpeedUnitsPerMs = 2;
    }

    Enemy::Enemy(int tileX, int tileY)
        : m_subX(tileX * kSubcell)
        , m_subY(tileY * kSubcell)
        , m_targetSubX(tileX * kSubcell)
        , m_targetSubY(tileY * kSubcell)
    {
    }

    bool Enemy::integrate(const IGame &game, IRng &rng, int deltaMs)
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

    std::optional<Direction> Enemy::randomWalkableDir(const IGame &game, IRng &rng) const
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
        case EnemyType::Bouncer:
            return std::make_unique<Bouncer>(tileX, tileY);
        case EnemyType::Wanderer:
            break;
        }
        return std::make_unique<Wanderer>(tileX, tileY);
    }
} // namespace pyrelite
