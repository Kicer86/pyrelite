
#pragma once

#include "grid.h"

namespace pyrelite
{
    // Width/height (in tiles) of one world chunk. Even on purpose, so the GLOBAL
    // even/even pillar lattice stays continuous across chunk seams: a chunk's origin
    // column and row (chunkX*kChunkSize, chunkY*kChunkSize) are always even, so local
    // parity matches global parity in every chunk.
    inline constexpr int kChunkSize = 16;

    // The character of a chunk's interior. The world generator draws one per chunk
    // from its seed; it sets the brick density and structure. A single switch seam
    // (mirrors the enemy archetypes) so a new biome is one enum value + one fill rule.
    enum class Biome { Plaza, Thicket, Corridor, Rooms };
    inline constexpr int kBiomeCount = 4;

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
