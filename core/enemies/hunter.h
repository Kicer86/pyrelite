
#pragma once

#include <optional>

#include "enemies/enemy.h"
#include "grid/movement.h"

namespace pyrelite
{
    // Smart pursuit: breadth-first searches the walkable tiles for the shortest route
    // to the player and takes its first step, so it threads corners and detours that
    // the greedy Chaser cannot — it will even step away from the player to round an
    // obstacle. When the player is unreachable it roams (like the Chaser) rather than
    // freezing against the wall between them.
    class Hunter : public Enemy
    {
    public:
        using Enemy::Enemy;

        EnemyType type() const override { return EnemyType::Hunter; }

    protected:
        std::optional<Direction> chooseDirection(const IGame &game, IRng &rng) override;
    };
} // namespace pyrelite
