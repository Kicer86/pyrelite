
#pragma once

#include "grid/grid.h"

namespace pyrelite
{
    // Width/height (in tiles) of one world chunk.
    inline constexpr int kChunkSize = 16;

    // The chunk coordinate that contains a global tile (floored, so it is correct on
    // both sides of zero: chunkOf(-1) == -1, not 0). kChunkSize is always positive.
    inline int chunkOf(int globalTile)
    {
        const int q = globalTile / kChunkSize;
        const int r = globalTile % kChunkSize;
        return (r != 0 && r < 0) ? q - 1 : q;
    }

    // The interior STYLE of a generated province. Several neighbouring zones share a
    // biome, and every chunk cut from them reports it: a long open Hall, a chamber-heavy
    // Warren, a Pillar island field, a brick-dense Thicket, or a wide Cavern. Style
    // changes room and decoration parameters; the graph that guarantees connectivity
    // remains style-independent.
    enum class Biome { Hall, Warren, Pillars, Thicket, Cavern };
    inline constexpr int kBiomeCount = 5;

    // A generated kChunkSize x kChunkSize block of the world at chunk coordinate
    // (chunkX, chunkY). Tiles are addressed in LOCAL coordinates [0, kChunkSize):
    // the global tile (chunkX*kChunkSize + lx, chunkY*kChunkSize + ly) maps to local
    // (lx, ly). Pure data — the generation rules live in world_gen.
    class Chunk
    {
    public:
        Chunk(int chunkX, int chunkY, Biome biome)
            : m_chunkX(chunkX)
            , m_chunkY(chunkY)
            , m_biome(biome)
            , m_tiles(kChunkSize, kChunkSize)
        {
        }

        int chunkX() const { return m_chunkX; }
        int chunkY() const { return m_chunkY; }
        Biome biome() const { return m_biome; }

        Tile at(int localX, int localY) const { return m_tiles.at(localX, localY); }
        void set(int localX, int localY, Tile tile) { m_tiles.set(localX, localY, tile); }

    private:
        int m_chunkX;
        int m_chunkY;
        Biome m_biome;
        Grid m_tiles;
    };
} // namespace pyrelite
