
#pragma once

#include <cstdint>

#include "world/chunk.h"

namespace pyrelite
{
    // Deterministically generate the world chunk at (chunkX, chunkY) for a world seed.
    // Pure function: the same (seed, chunkX, chunkY) always yields the identical chunk
    // on every platform, so the streamed world is reproducible and any chunk can be
    // materialized on demand without its neighbours.
    //
    // Chunks are storage/streaming slices only. Geometry is designed in larger 4x4-chunk
    // ZONES, removing the artificial rock frame at every chunk edge. Each zone contains
    // curved, variable-width passages and irregular rooms cut through rock. The rock
    // lining them is a thin BANK (Wall with some bomb-through Brick); everything deeper
    // is VOID, visible behind the bank but never reachable.
    //
    // Connectivity is guaranteed BY CONSTRUCTION. Every non-origin zone has one parent
    // edge leading toward the origin; seeded optional edges add loops. Portal position
    // and width come from the SHARED zone-boundary identity, so independently generated
    // neighbours agree exactly. All portals and rooms inside a zone are joined by a
    // deterministic spanning tree plus optional local loops. The whole world's floor is
    // therefore one component walkable without bombing.
    Chunk generateChunk(std::uint64_t seed, int chunkX, int chunkY);

    // The difficulty/theme TIER of a chunk, rising with distance from the origin so the
    // world grows tighter, emptier and more fantastic the farther the player travels.
    // This is the single escalation-policy seam: it currently steps in radial rings, but
    // changing the policy (e.g. to a smooth gradient) is a change to this one function
    // plus the tier table in world_gen.cpp. The view reads it to pick a region palette.
    int worldTier(int chunkX, int chunkY);
} // namespace pyrelite
