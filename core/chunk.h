
#pragma once

#include "grid.h"

namespace pyrelite
{
    // Width/height (in tiles) of one world chunk. Even on purpose, so the edge-midpoint
    // doorway (kChunkSize/2) sits on an exact cell and lines up with the neighbour's
    // doorway across every seam — the world stays traversable chunk-to-chunk by
    // construction.
    inline constexpr int kChunkSize = 16;

    // The character of a chunk's chamber. The world generator draws one per chunk from
    // its seed; it sets the interior cover and layout (a Rooms chunk subdivides into
    // sub-rooms, Pillars is a pillar field, Thicket a dense brick maze, and so on). A
    // single switch seam (mirrors the enemy archetypes) so a new kind is one enum value
    // + one fill rule. Every chamber, whatever its kind, is walled with guaranteed open
    // doorways and a clear central spine, so connectivity never depends on the kind.
    enum class Biome { Hall, Rooms, Pillars, Thicket, Plaza };
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
