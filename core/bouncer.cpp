
#include "bouncer.h"

#include "igame.h"
#include "irng.h"

namespace pyrelite
{
    std::optional<Direction> Bouncer::chooseDirection(const IGame &game, IRng &)
    {
        const auto walkableAlong = [&](Direction d)
        {
            int nx = tileX();
            int ny = tileY();
            stepTile(d, nx, ny);
            return canEnter(game, nx, ny);
        };

        if (walkableAlong(dir()))
            return dir(); // hold the line

        const Direction back = reverse(dir());
        if (walkableAlong(back))
            return back; // bounce straight off the wall

        // Boxed front and back (an inside corner): deflect to the first open side, in a
        // fixed order so the patrol stays reproducible without ever touching the RNG.
        for (const Direction d : {Direction::Up, Direction::Down,
                 Direction::Left, Direction::Right})
        {
            if (walkableAlong(d))
                return d;
        }
        return std::nullopt; // boxed in
    }
} // namespace pyrelite
