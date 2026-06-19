
#include "wanderer.h"

#include "igame.h"
#include "irng.h"

namespace pyrelite
{
    std::optional<Direction> Wanderer::chooseDirection(const IGame &game, IRng &rng)
    {
        int ahead = tileX();
        int aheadY = tileY();
        stepTile(dir(), ahead, aheadY);
        if (game.walkable(ahead, aheadY))
            return dir(); // keep going straight
        return randomWalkableDir(game, rng);
    }
} // namespace pyrelite
