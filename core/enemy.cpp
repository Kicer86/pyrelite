
#include "enemy.h"

#include "bouncer.h"
#include "chaser.h"
#include "ghost.h"
#include "hunter.h"
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
        : m_mover(tileX, tileY)
    {
    }

    bool Enemy::integrate(const IGame &game, IRng &rng, int deltaMs)
    {
        if (m_mover.centred())
        {
            const std::optional<Direction> next = chooseDirection(game, rng);
            if (!next)
                return false; // boxed in

            m_dir = *next;
            int ahead = m_mover.tileX();
            int aheadY = m_mover.tileY();
            stepTile(m_dir, ahead, aheadY);
            m_mover.aimAt(ahead, aheadY);
        }

        return m_mover.advance(kEnemySpeedUnitsPerMs * deltaMs);
    }

    bool Enemy::canEnter(const IGame &game, int x, int y) const
    {
        return game.walkable(x, y);
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
            if (canEnter(game, nx, ny))
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
        case EnemyType::Hunter:
            return std::make_unique<Hunter>(tileX, tileY);
        case EnemyType::Ghost:
            return std::make_unique<Ghost>(tileX, tileY);
        case EnemyType::Wanderer:
            break;
        }
        return std::make_unique<Wanderer>(tileX, tileY);
    }
} // namespace pyrelite
