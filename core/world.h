
#pragma once

#include <cstdint>
#include <map>
#include <utility>

#include "chunk.h"
#include "delta_store.h"
#include "grid.h"
#include "iterrain.h"

namespace pyrelite
{
    // An infinite, seeded world streamed a window at a time. Chunks are generated
    // deterministically on demand (generateChunk) and cached; a DeltaStore overlay
    // makes the player's terrain changes permanent across reloads (anti-farm). Only
    // the chunks near the player are kept resident — the rest are dropped, since the
    // base is regenerable and the deltas persist. Coordinates are GLOBAL tiles.
    //
    // It is the streamed ITerrain the playing Game runs on (BoundedTerrain is the
    // fixed-arena counterpart). The chunk cache is logically transparent, so reads are
    // const: at() materializes on demand into a mutable cache.
    class World : public ITerrain
    {
    public:
        explicit World(std::uint64_t seed);

        // Terrain at a global tile, materializing the containing chunk if needed.
        Tile at(int globalX, int globalY) const override;

        // Overwrite a terrain tile (a player change, e.g. a bombed brick) and record
        // it so it survives a chunk unload/reload.
        void set(int globalX, int globalY, Tile tile) override;

        // The world is unbounded — every coordinate is in-bounds.
        bool inBounds(int, int) const override { return true; }

        // Stream the window around a global tile (the player); ITerrain hook.
        void stream(int centerX, int centerY) override;

        // Keep every chunk within `radius` chunks (Chebyshev) of the centre chunk
        // resident and drop the rest — call as the player moves between chunks.
        void ensureWindow(int centerChunkX, int centerChunkY, int radius) const;

        // The chunk coordinate that contains a global tile (floored, so negative
        // coordinates map correctly: chunkCoord(-1) == -1, not 0).
        static int chunkCoord(int globalTile);

        std::size_t residentChunkCount() const { return m_chunks.size(); }
        std::size_t deltaCount() const { return m_deltas.size(); }

    private:
        Chunk &ensureResident(int chunkX, int chunkY) const;

        std::uint64_t m_seed;
        mutable std::map<std::pair<int, int>, Chunk> m_chunks;
        DeltaStore m_deltas;
    };
} // namespace pyrelite
