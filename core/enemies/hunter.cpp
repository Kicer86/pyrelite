
#include "enemies/hunter.h"

#include <queue>
#include <set>
#include <utility>

#include "game/igame.h"
#include "rng/irng.h"

namespace pyrelite
{
    namespace
    {
        // Safety bound on the flood: a real arena is walled, so the search terminates
        // naturally, but this stops a pathological/unbounded world from looping forever.
        constexpr int kMaxSearchCells = 8192;

        constexpr Direction kDirs[4] = {Direction::Up, Direction::Down,
                                        Direction::Left, Direction::Right};
    }

    std::optional<Direction> Hunter::chooseDirection(const IGame &game, IRng &rng)
    {
        const int sx = tileX();
        const int sy = tileY();
        const int gx = game.playerX();
        const int gy = game.playerY();
        if (sx == gx && sy == gy)
            return std::nullopt; // already sharing the player's tile (a contact death)

        // Breadth-first search outward from our tile. Each frontier cell remembers the
        // very first step taken to reach it; the first time we touch the player's tile,
        // that remembered step is the head of a shortest route. Neighbours are expanded
        // in a fixed order, so ties break deterministically and the path needs no RNG.
        struct Node { int x; int y; Direction first; };
        std::queue<Node> frontier;
        std::set<std::pair<int, int>> seen;
        seen.emplace(sx, sy);

        for (const Direction d : kDirs)
        {
            int nx = sx;
            int ny = sy;
            stepTile(d, nx, ny);
            if (!canEnter(game, nx, ny) || seen.contains({nx, ny}))
                continue;
            if (nx == gx && ny == gy)
                return d; // the player is right next door
            seen.emplace(nx, ny);
            frontier.push({nx, ny, d});
        }

        int budget = kMaxSearchCells;
        while (!frontier.empty() && budget-- > 0)
        {
            const Node cur = frontier.front();
            frontier.pop();
            for (const Direction d : kDirs)
            {
                int nx = cur.x;
                int ny = cur.y;
                stepTile(d, nx, ny);
                if (!canEnter(game, nx, ny) || seen.contains({nx, ny}))
                    continue;
                if (nx == gx && ny == gy)
                    return cur.first; // reached the player; head down this route
                seen.emplace(nx, ny);
                frontier.push({nx, ny, cur.first});
            }
        }

        return randomWalkableDir(game, rng); // no route to the player — roam instead
    }
} // namespace pyrelite
