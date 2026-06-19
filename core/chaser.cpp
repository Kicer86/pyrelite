
#include "chaser.h"

#include <cstdlib>

#include "game.h"

namespace pyrelite
{
    std::optional<Direction> Chaser::chooseDirection(const Game &game, Rng &rng)
    {
        const int tx = tileX();
        const int ty = tileY();
        const int dx = game.playerX() - tx;
        const int dy = game.playerY() - ty;
        const Direction horiz = dx > 0 ? Direction::Right : Direction::Left;
        const Direction vert = dy > 0 ? Direction::Down : Direction::Up;

        // Prefer the axis with the larger gap, then the other; skip an axis already
        // aligned so we never "step toward" a zero delta.
        Direction prefs[2];
        int n = 0;
        if (std::abs(dx) >= std::abs(dy))
        {
            if (dx != 0)
                prefs[n++] = horiz;
            if (dy != 0)
                prefs[n++] = vert;
        }
        else
        {
            if (dy != 0)
                prefs[n++] = vert;
            if (dx != 0)
                prefs[n++] = horiz;
        }

        for (int i = 0; i < n; ++i)
        {
            int nx = tx;
            int ny = ty;
            stepTile(prefs[i], nx, ny);
            if (game.walkable(nx, ny))
                return prefs[i];
        }
        return randomWalkableDir(game, rng);
    }
} // namespace pyrelite
