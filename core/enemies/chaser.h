
#pragma once

#include <optional>

#include "enemies/enemy.h"
#include "grid/movement.h"

namespace pyrelite
{
    // Greedy pursuit: steps toward the player on the axis with the larger gap, then
    // the other axis; when both are blocked it roams to route around the obstacle.
    class Chaser : public Enemy
    {
    public:
        using Enemy::Enemy;

        EnemyType type() const override { return EnemyType::Chaser; }

    protected:
        std::optional<Direction> chooseDirection(const IGame &game, IRng &rng) override;
    };
} // namespace pyrelite
