
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "world/chunk.h"
#include "world/world_gen.h"
#include "world/zone.h"

namespace pyrelite
{
    // A caller-owned memo over zone generation. generateChunk builds a whole zone and
    // slices one chunk out of it, so materializing a zone chunk by chunk would rerun the
    // same generation kZoneChunks^2 times (and rerun it again after eviction/reload).
    // Hold a ZoneCache and slice chunks from it so each zone is built once. The core
    // generators stay pure (no global state); the cache is keyed on the full
    // (seed, zoneX, zoneY) input, so it is observable only as speed. Single-threaded use
    // (Qt main thread / single-threaded WASM / sequential tests), so no synchronisation.
    class ZoneCache
    {
    public:
        // The capacity comfortably exceeds the zones a streaming (kStreamRadius) or
        // preview window can touch at once, so the working set never thrashes.
        explicit ZoneCache(std::size_t capacity = 16)
            : m_capacity(capacity)
        {
        }

        Chunk chunk(std::uint64_t seed, int chunkX, int chunkY)
        {
            return zoneFor(seed, Zone::ofChunk(chunkX), Zone::ofChunk(chunkY))
                .chunk(chunkX, chunkY);
        }

    private:
        struct Entry
        {
            std::uint64_t seed;
            int zoneX;
            int zoneY;
            Zone zone;
        };

        const Zone &zoneFor(std::uint64_t seed, int zoneX, int zoneY)
        {
            for (const Entry &entry : m_entries)
                if (entry.seed == seed && entry.zoneX == zoneX && entry.zoneY == zoneY)
                    return entry.zone;

            if (m_entries.size() >= m_capacity)
                m_entries.erase(m_entries.begin());
            m_entries.push_back({seed, zoneX, zoneY, generateZone(seed, zoneX, zoneY)});
            return m_entries.back().zone;
        }

        std::size_t m_capacity;
        std::vector<Entry> m_entries;
    };
} // namespace pyrelite
