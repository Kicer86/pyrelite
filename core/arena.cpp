
#include "arena.h"

#include "rng.h"

namespace pyrelite {

namespace {

constexpr int kBrickPercent = 80;

bool isBorder(int x, int y, int width, int height)
{
    return x == 0 || y == 0 || x == width - 1 || y == height - 1;
}

bool isPillar(int x, int y)
{
    return x % 2 == 0 && y % 2 == 0;
}

} // namespace

Grid generateArena(int width, int height, std::uint64_t seed)
{
    Grid grid(width, height);
    Rng rng(seed);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (isBorder(x, y, width, height) || isPillar(x, y))
                grid.set(x, y, Tile::Wall);
            else if (rng.chance(kBrickPercent))
                grid.set(x, y, Tile::Brick);
            // otherwise leave it Empty (the grid's default)
        }
    }

    // Keep the spawn corner clear so the player is never boxed in.
    grid.set(1, 1, Tile::Empty);
    grid.set(2, 1, Tile::Empty);
    grid.set(1, 2, Tile::Empty);

    return grid;
}

} // namespace pyrelite
