
#pragma once

#include <memory>

#include "game/igame.h"
#include "rng/irng.h"

namespace pyrelite
{
    enum class EnemyType { Wanderer, Chaser, Bouncer, Hunter, Ghost };

    // A moving hazard as the rest of the game sees it: a position (sub-cell and tile)
    // an archetype tag, and a per-tick update. Pure interface — concrete archetypes
    // implement it through the shared Enemy base; consumers (Game, the view) depend
    // only on this.
    class IEnemy
    {
    public:
        virtual ~IEnemy() = default;

        // Position in sub-units (kSubcell per tile) for smooth rendering, and as the
        // tile currently occupied (nearest centre) for collision and pick-ups.
        virtual int subX() const = 0;
        virtual int subY() const = 0;
        virtual int tileX() const = 0;
        virtual int tileY() const = 0;

        virtual EnemyType type() const = 0;

        // Advance one tick within the world; returns true if the enemy moved.
        virtual bool integrate(const IGame &game, IRng &rng, int deltaMs) = 0;
    };

    // Build a concrete enemy of the given archetype, centred on tile (tileX, tileY).
    std::unique_ptr<IEnemy> makeEnemy(EnemyType type, int tileX, int tileY);
} // namespace pyrelite
