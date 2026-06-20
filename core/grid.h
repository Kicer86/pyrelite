
#pragma once

#include <vector>

namespace pyrelite
{
    // A single cell of the playfield.
    enum class Tile
    {
        Empty,
        Wall,   // indestructible
        Brick,  // destructible, may drop a power-up
    };

    // The game playfield: a fixed-size rectangular grid of tiles.
    // Pure logic, no rendering or Qt dependency.
    class Grid
    {
    public:
        Grid(int width, int height);

        int width() const { return m_width; }
        int height() const { return m_height; }

        bool inBounds(int x, int y) const;
        Tile at(int x, int y) const;
        void set(int x, int y, Tile tile);

    private:
        int index(int x, int y) const { return y * m_width + x; }

        int m_width;
        int m_height;
        std::vector<Tile> m_tiles;
    };
} // namespace pyrelite
