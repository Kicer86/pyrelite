
#pragma once

#include "grid/grid.h"
#include "world/chunk.h"

namespace pyrelite
{
    // Geometry is designed in 4x4-chunk ZONES; chunks are streaming slices of them.
    // These are public so callers can cache at the (expensive) zone level instead of
    // rebuilding a whole zone for each of its chunks — see ZoneCache.
    inline constexpr int kZoneChunks = 4;
    inline constexpr int kZoneSize = kZoneChunks * kChunkSize;

    // A generated kZoneSize x kZoneSize block of the world at zone coordinate
    // (zoneX, zoneY). Zones are centred on the origin so the spawn does not sit on a
    // zone boundary: zone 0 owns chunks [-kZoneChunks/2, kZoneChunks/2). Building a zone
    // is the expensive generation step (world_gen); each chunk is then sliced out
    // cheaply. Pure data — the generation rules live in world_gen.
    class Zone
    {
    public:
        Zone(int zoneX, int zoneY, Biome biome)
            : m_zoneX(zoneX)
            , m_zoneY(zoneY)
            , m_biome(biome)
            , m_tiles(kZoneSize, kZoneSize)
        {
            // Generation carves passages out of solid rock, so start fully walled.
            for (int y = 0; y < kZoneSize; ++y)
                for (int x = 0; x < kZoneSize; ++x)
                    m_tiles.set(x, y, Tile::Wall);
        }

        int zoneX() const { return m_zoneX; }
        int zoneY() const { return m_zoneY; }
        Biome biome() const { return m_biome; }

        Tile at(int localX, int localY) const { return m_tiles.at(localX, localY); }
        void set(int localX, int localY, Tile tile) { m_tiles.set(localX, localY, tile); }

        // The zone coordinate that owns a chunk coordinate (centred, floored so it is
        // correct on both sides of zero).
        static int ofChunk(int chunkCoord)
        {
            const int shifted = chunkCoord + kZoneChunks / 2;
            const int q = shifted / kZoneChunks;
            const int r = shifted % kZoneChunks;
            return r < 0 ? q - 1 : q;
        }

        // The lowest chunk coordinate owned by a zone.
        static int minChunk(int zoneCoord)
        {
            return zoneCoord * kZoneChunks - kZoneChunks / 2;
        }

        // Slice out the chunk at GLOBAL chunk coordinates (chunkX, chunkY), which this
        // zone must own (ofChunk(chunkX) == zoneX()).
        Chunk chunk(int chunkX, int chunkY) const
        {
            Chunk result(chunkX, chunkY, m_biome);
            const int sourceX = (chunkX - minChunk(m_zoneX)) * kChunkSize;
            const int sourceY = (chunkY - minChunk(m_zoneY)) * kChunkSize;
            for (int y = 0; y < kChunkSize; ++y)
                for (int x = 0; x < kChunkSize; ++x)
                    result.set(x, y, at(sourceX + x, sourceY + y));
            return result;
        }

    private:
        int m_zoneX;
        int m_zoneY;
        Biome m_biome;
        Grid m_tiles;
    };
} // namespace pyrelite
