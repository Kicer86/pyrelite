
#pragma once

#include <optional>

#include "enemy.h"
#include "movement.h"

namespace pyrelite
{
    // Ballistic ricochet: holds a straight heading until a wall stops it, then bounces
    // straight back the way it came (reflecting off the wall). Only when both ahead and
    // behind are blocked — an inside corner — does it deflect to an open side. Fully
    // deterministic: unlike the Wanderer it never rolls the dice, so it patrols a clean,
    // readable line instead of meandering. Unaware of the player.
    class Bouncer : public Enemy
    {
    public:
        using Enemy::Enemy;

        EnemyType type() const override { return EnemyType::Bouncer; }

    protected:
        std::optional<Direction> chooseDirection(const IGame &game, IRng &rng) override;
    };
} // namespace pyrelite
