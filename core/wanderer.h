
#pragma once

#include <optional>

#include "enemy.h"
#include "movement.h"

namespace pyrelite
{
    // Roams at random: keeps its current heading while the tile ahead is walkable,
    // otherwise turns to a uniformly random walkable direction (so it bounces off
    // walls and meanders). Unaware of the player.
    class Wanderer : public IEnemy
    {
    public:
        using IEnemy::IEnemy;

        EnemyType type() const override { return EnemyType::Wanderer; }

    protected:
        std::optional<Direction> chooseDirection(const IGame &game, IRng &rng) override;
    };
} // namespace pyrelite
