
#include "grid/grid.h"

#include <stdexcept>

namespace pyrelite
{
    Grid::Grid(int width, int height)
        : m_width(width)
        , m_height(height)
    {
        if (width <= 0 || height <= 0)
            throw std::invalid_argument("Grid dimensions must be positive");

        m_tiles.assign(static_cast<std::size_t>(width) * height, Tile::Empty);
    }

    bool Grid::inBounds(int x, int y) const
    {
        return x >= 0 && x < m_width && y >= 0 && y < m_height;
    }

    Tile Grid::at(int x, int y) const
    {
        if (!inBounds(x, y))
            throw std::out_of_range("Grid::at out of bounds");
        return m_tiles[index(x, y)];
    }

    void Grid::set(int x, int y, Tile tile)
    {
        if (!inBounds(x, y))
            throw std::out_of_range("Grid::set out of bounds");
        m_tiles[index(x, y)] = tile;
    }
} // namespace pyrelite
