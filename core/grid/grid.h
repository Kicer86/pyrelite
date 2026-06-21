
#pragma once

#include <vector>

namespace pyrelite
{
    // A single cell of the playfield.
    enum class Tile
    {
        Empty,
        Wall,   // indestructible rock bank
        Brick,  // destructible, may drop a power-up
        Void,   // the abyss behind the rock: solid and indestructible like Wall, but
                // generated as deep filler beyond the banks and drawn as empty space.
                // Always sits behind at least one Wall, so it is never reachable.
    };

    // Solid, indestructible terrain that blocks movement and stops a blast. Wall and
    // Void are gameplay-identical (only their look and generation intent differ), so
    // callers should test this rather than `== Tile::Wall`.
    inline bool isSolid(Tile t) { return t == Tile::Wall || t == Tile::Void; }

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
