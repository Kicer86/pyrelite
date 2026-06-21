
#pragma once

#include "grid/grid.h"

namespace pyrelite
{
    // The terrain the simulation runs on, abstracted so the same Game works over a
    // fixed bounded arena (BoundedTerrain — used by the headless mechanics tests) and
    // the infinite streamed World (used in play). Coordinates are tile coordinates:
    // 0-based for a bounded arena, global for the World.
    class ITerrain
    {
    public:
        virtual ~ITerrain() = default;

        // The tile at (x, y). Total: out-of-world coordinates read as Wall (solid), so
        // callers need not bounds-check before reading.
        virtual Tile at(int x, int y) const = 0;

        // Overwrite a tile (e.g. a bombed brick). For the streamed World this also
        // records a persistent delta, so the change survives a chunk unload/reload.
        virtual void set(int x, int y, Tile tile) = 0;

        // Whether (x, y) lies inside the playable area: always true for the infinite
        // World, bounded for a fixed arena.
        virtual bool inBounds(int x, int y) const = 0;

        // Keep the terrain around tile (centerX, centerY) resident. A no-op for a fully
        // in-memory bounded arena; the World streams its window here.
        virtual void stream(int /*centerX*/, int /*centerY*/) {}

        // Whether simulation work at this tile is active. Bounded terrain is fully
        // active; streamed terrain limits entities and pathfinding to the player window.
        virtual bool simulationActiveAt(int x, int y) const
        {
            return inBounds(x, y);
        }

        // Visible global tile bounds, inclusive. Streamed terrain retains this region
        // alongside the simulation window; bounded terrain needs no special handling.
        virtual void setVisibleArea(int /*minX*/, int /*minY*/,
                                    int /*maxX*/, int /*maxY*/) {}
    };
} // namespace pyrelite
