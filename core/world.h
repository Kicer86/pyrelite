
#pragma once

#include <cstdint>
#include <map>
#include <utility>

#include "chunk.h"
#include "delta_store.h"
#include "grid.h"

namespace pyrelite
{
    // An infinite, seeded world streamed a window at a time. Chunks are generated
    // deterministically on demand (generateChunk) and cached; a DeltaStore overlay
    // makes the player's terrain changes permanent across reloads (anti-farm). Only
    // the chunks near the player are kept resident — the rest are dropped, since the
    // base is regenerable and the deltas persist. Coordinates are GLOBAL tiles.
    class World
    {
    public:
        explicit World(std::uint64_t seed);

        // Terrain at a global tile, materializing the containing chunk if needed.
        Tile at(int globalX, int globalY);

        // Overwrite a terrain tile (a player change, e.g. a bombed brick) and record
        // it so it survives a chunk unload/reload.
        void set(int globalX, int globalY, Tile tile);

        // Keep every chunk within `radius` chunks (Chebyshev) of the centre chunk
        // resident and drop the rest — call as the player moves between chunks.
        void ensureWindow(int centerChunkX, int centerChunkY, int radius);

        // The chunk coordinate that contains a global tile (floored, so negative
        // coordinates map correctly: chunkCoord(-1) == -1, not 0).
        static int chunkCoord(int globalTile);

        std::size_t residentChunkCount() const { return m_chunks.size(); }
        std::size_t deltaCount() const { return m_deltas.size(); }

    private:
        Chunk &ensureResident(int chunkX, int chunkY);

        std::uint64_t m_seed;
        std::map<std::pair<int, int>, Chunk> m_chunks;
        DeltaStore m_deltas;
    };
} // namespace pyrelite
