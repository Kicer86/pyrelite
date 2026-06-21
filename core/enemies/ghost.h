
#pragma once

#include "enemies/chaser.h"
#include "game/igame.h"

namespace pyrelite
{
    // A Chaser that phases through bricks: it pursues the player exactly like the
    // greedy Chaser, but only solid walls (and bombs) stop it — destructible bricks
    // do not. So it beelines through the rubble where the others must go around, which
    // makes it relentless in the brick-dense arena. Still killed by explosions like
    // any enemy. Reuses the entire Chaser strategy, overriding only what it may enter.
    class Ghost : public Chaser
    {
    public:
        using Chaser::Chaser;

        EnemyType type() const override { return EnemyType::Ghost; }

    protected:
        bool canEnter(const IGame &game, int x, int y) const override
        {
            return game.walkableThroughBricks(x, y);
        }
    };
} // namespace pyrelite
