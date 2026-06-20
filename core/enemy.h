
#pragma once

#include <optional>

#include "ienemy.h"
#include "igame.h"
#include "irng.h"
#include "movement.h"

namespace pyrelite
{
    // Shared implementation behind IEnemy: owns the sub-cell position and the
    // grid-locked movement integration common to every archetype (travel tile to
    // tile, only choosing a new heading once centred). Subclasses customise only
    // chooseDirection() and declare their type(); abstract, never instantiated alone.
    class Enemy : public IEnemy
    {
    public:
        Enemy(int tileX, int tileY);

        int subX() const override { return m_subX; }
        int subY() const override { return m_subY; }
        int tileX() const override { return tileOf(m_subX); }
        int tileY() const override { return tileOf(m_subY); }

        // When centred, pick a new heading (per archetype) and commit to that
        // neighbouring tile, then crawl toward it. With nowhere to go it sits still.
        bool integrate(const IGame &game, IRng &rng, int deltaMs) override;

    protected:
        Direction dir() const { return m_dir; }

        // The one thing each archetype customizes: choose the next heading from the
        // current (centred) tile, or nullopt if boxed in.
        virtual std::optional<Direction> chooseDirection(const IGame &game, IRng &rng) = 0;

        // Which tiles this archetype may move into — the single passability rule every
        // archetype's navigation consults. Defaults to the game's walkable rule; the
        // Ghost overrides it to pass through bricks.
        virtual bool canEnter(const IGame &game, int x, int y) const;

        // A uniformly random enterable orthogonal neighbour of the current tile, or
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
} // namespace pyrelite
