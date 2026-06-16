
#include "grid.h"

#include <stdexcept>

namespace pyrelite {

Grid::Grid(int width, int height)
    : width_(width), height_(height)
{
    if (width <= 0 || height <= 0)
        throw std::invalid_argument("Grid dimensions must be positive");

    tiles_.assign(static_cast<std::size_t>(width) * height, Tile::Empty);
}

bool Grid::inBounds(int x, int y) const
{
    return x >= 0 && x < width_ && y >= 0 && y < height_;
}

Tile Grid::at(int x, int y) const
{
    if (!inBounds(x, y))
        throw std::out_of_range("Grid::at out of bounds");
    return tiles_[index(x, y)];
}

void Grid::set(int x, int y, Tile tile)
{
    if (!inBounds(x, y))
        throw std::out_of_range("Grid::set out of bounds");
    tiles_[index(x, y)] = tile;
}

} // namespace pyrelite
