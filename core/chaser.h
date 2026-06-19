
#pragma once

#include <optional>

#include "enemy.h"
#include "movement.h"

namespace pyrelite
{
    // Greedy pursuit: steps toward the player on the axis with the larger gap, then
    // the other axis; when both are blocked it roams to route around the obstacle.
    class Chaser : public IEnemy
    {
    public:
        using IEnemy::IEnemy;

        EnemyType type() const override { return EnemyType::Chaser; }

    protected:
        std::optional<Direction> chooseDirection(const Game &game, Rng &rng) override;
    };
} // namespace pyrelite
