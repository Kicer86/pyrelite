
#include "enemies/wanderer.h"

#include "game/igame.h"
#include "rng/irng.h"

namespace pyrelite
{
    std::optional<Direction> Wanderer::chooseDirection(const IGame &game, IRng &rng)
    {
        int ahead = tileX();
        int aheadY = tileY();
        stepTile(dir(), ahead, aheadY);
        if (canEnter(game, ahead, aheadY))
            return dir(); // keep going straight
        return randomWalkableDir(game, rng);
    }
} // namespace pyrelite
