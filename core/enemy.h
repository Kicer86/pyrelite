
#pragma once

#include <cstdint>
#include <memory>
#include <optional>

#include "igame.h"
#include "irng.h"
#include "movement.h"

namespace pyrelite
{
    enum class EnemyType { Wanderer, Chaser };

    // A moving hazard. Like the player it lives in sub-units (kSubcell per tile) and
    // is grid-locked: it travels tile to tile, only choosing a new heading once
    // centred. This interface owns the position and the shared movement integration;
    // each archetype is a subclass that only decides where to head (chooseDirection).
    class IEnemy
    {
    public:
        IEnemy(int tileX, int tileY);
        virtual ~IEnemy() = default;

        int subX() const { return m_subX; }
        int subY() const { return m_subY; }
        int tileX() const { return tileOf(m_subX); }
        int tileY() const { return tileOf(m_subY); }
        Direction dir() const { return m_dir; }

        virtual EnemyType type() const = 0;

        // Advance one tick: when centred, pick a new heading (per archetype) and
        // commit to that neighbouring tile, then crawl toward it. Returns true if it
        // moved. With nowhere to go it sits still until a brick is cleared.
        bool integrate(const IGame &game, IRng &rng, int deltaMs);

    protected:
        // The one thing each archetype customizes: choose the next heading from the
        // current (centred) tile, or nullopt if boxed in.
        virtual std::optional<Direction> chooseDirection(const IGame &game, IRng &rng) = 0;

        // A uniformly random walkable orthogonal neighbour of the current tile, or
        // nullopt when boxed in. Candidates are gathered in a fixed order before the
        // draw, so the choice is reproducible from the RNG stream.
        std::optional<Direction> randomWalkableDir(const IGame &game, IRng &rng) const;

    private:
        int m_subX;
        int m_subY;
        int m_targetSubX;
        int m_targetSubY;
        Direction m_dir = Direction::Down;
    };

    // Build a concrete enemy of the given archetype, centred on tile (tileX, tileY).
    std::unique_ptr<IEnemy> makeEnemy(EnemyType type, int tileX, int tileY);
} // namespace pyrelite
