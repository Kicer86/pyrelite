
#include "wanderer.h"

#include "game.h"

namespace pyrelite
{
    std::optional<Direction> Wanderer::chooseDirection(const Game &game, Rng &rng)
    {
        int ahead = tileX();
        int aheadY = tileY();
        stepTile(dir(), ahead, aheadY);
        if (game.walkable(ahead, aheadY))
            return dir(); // keep going straight
        return randomWalkableDir(game, rng);
    }
} // namespace pyrelite
