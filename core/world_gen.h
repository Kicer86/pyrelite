
#pragma once

#include <cstdint>

#include "chunk.h"

namespace pyrelite
{
    // Deterministically generate the world chunk at (chunkX, chunkY) for a world
    // seed. Pure function: the same (seed, chunkX, chunkY) always yields the identical
    // chunk on every platform, so the streamed world is reproducible and any chunk can
    // be materialized on demand without its neighbours.
    //
    // Each chunk is a CHAMBER: a hybrid wall ring (stone anchors + brick stretches) with
    // a guaranteed open doorway at the midpoint of every edge, and an Empty central
    // spine joining the four doorways. The doorways sit at fixed cells, so a chamber's
    // spine meets its neighbours' across every seam — the world is traversable
    // chunk-to-chunk WITHOUT bombing, by construction. The chamber's kind (Biome) then
    // decorates the interior for variety: open hall, subdivided rooms, pillar field,
    // brick thicket, plaza. Interior stone only ever lands on isolated pillar slots, so
    // it can never seal a pocket; the whole world's non-Wall tiles stay one connected
    // component (a brick is bomb-through, so it never partitions anything either).
    Chunk generateChunk(std::uint64_t seed, int chunkX, int chunkY);
} // namespace pyrelite
