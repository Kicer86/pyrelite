
#include "world.h"

#include <algorithm>
#include <cstdlib>

#include "world_gen.h"

namespace pyrelite
{
    namespace
    {
        // Chunks kept resident around the player: a (2r+1)² window. Generous enough to
        // cover the viewport plus margin so streaming is never visible at the edges.
        constexpr int kStreamRadius = 2;

        // Floor division: rounds toward negative infinity, unlike C++'s truncating
        // `/`. Needed so a global tile maps to the SAME chunk on both sides of zero
        // (e.g. tile -1 belongs to chunk -1, not chunk 0).
        int floorDiv(int a, int b)
        {
            const int q = a / b;
            const int r = a % b;
            return (r != 0 && (r < 0) != (b < 0)) ? q - 1 : q;
        }
    }

    World::World(std::uint64_t seed)
        : m_seed(seed)
    {
    }

    int World::chunkCoord(int globalTile)
    {
        return floorDiv(globalTile, kChunkSize);
    }

    void World::stream(int centerX, int centerY)
    {
        ensureWindow(chunkCoord(centerX), chunkCoord(centerY), kStreamRadius);
    }

    Chunk &World::ensureResident(int chunkX, int chunkY) const
    {
        const auto key = std::make_pair(chunkX, chunkY);
        const auto it = m_chunks.find(key);
        if (it != m_chunks.end())
            return it->second;

        Chunk chunk = generateChunk(m_seed, chunkX, chunkY);

        // Reapply any recorded player changes that fall inside this chunk, so a
        // reloaded chunk reflects what the player did, not a fresh generation.
        const int originX = chunkX * kChunkSize;
        const int originY = chunkY * kChunkSize;
        for (int ly = 0; ly < kChunkSize; ++ly)
            for (int lx = 0; lx < kChunkSize; ++lx)
                if (const auto delta = m_deltas.at(originX + lx, originY + ly))
                    chunk.set(lx, ly, *delta);

        return m_chunks.emplace(key, std::move(chunk)).first->second;
    }

    Tile World::at(int globalX, int globalY) const
    {
        const int cx = chunkCoord(globalX);
        const int cy = chunkCoord(globalY);
        const Chunk &chunk = ensureResident(cx, cy);
        return chunk.at(globalX - cx * kChunkSize, globalY - cy * kChunkSize);
    }

    void World::set(int globalX, int globalY, Tile tile)
    {
        const int cx = chunkCoord(globalX);
        const int cy = chunkCoord(globalY);
        Chunk &chunk = ensureResident(cx, cy);
        chunk.set(globalX - cx * kChunkSize, globalY - cy * kChunkSize, tile);
        m_deltas.record(globalX, globalY, tile);
    }

    void World::ensureWindow(int centerChunkX, int centerChunkY, int radius) const
    {
        for (int cy = centerChunkY - radius; cy <= centerChunkY + radius; ++cy)
            for (int cx = centerChunkX - radius; cx <= centerChunkX + radius; ++cx)
                ensureResident(cx, cy);

        // Drop chunks outside the window (Chebyshev distance > radius). The base is
        // regenerable and deltas persist in the store, so nothing is lost.
        for (auto it = m_chunks.begin(); it != m_chunks.end();)
        {
            const int dist = std::max(std::abs(it->first.first - centerChunkX),
                                      std::abs(it->first.second - centerChunkY));
            if (dist > radius)
                it = m_chunks.erase(it);
            else
                ++it;
        }
    }
} // namespace pyrelite
