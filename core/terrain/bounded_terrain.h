
#pragma once

#include <utility>

#include "grid/grid.h"
#include "terrain/iterrain.h"

namespace pyrelite
{
    // A fixed-size arena held fully in memory. Out-of-bounds reads as Wall, so the
    // arena edge is solid terrain. Used by the headless mechanics tests (hand-authored
    // grids) and the classic bounded game.
    class BoundedTerrain : public ITerrain
    {
    public:
        explicit BoundedTerrain(Grid grid)
            : m_grid(std::move(grid))
        {
        }

        Tile at(int x, int y) const override
        {
            return m_grid.inBounds(x, y) ? m_grid.at(x, y) : Tile::Wall;
        }

        void set(int x, int y, Tile tile) override { m_grid.set(x, y, tile); }

        bool inBounds(int x, int y) const override { return m_grid.inBounds(x, y); }

        int width() const { return m_grid.width(); }
        int height() const { return m_grid.height(); }

    private:
        Grid m_grid;
    };
} // namespace pyrelite
