
#pragma once

#include <cstdint>

#include "world/chunk.h"

namespace pyrelite
{
    // Deterministically generate the world chunk at (chunkX, chunkY) for a world
    // seed. Pure function: the same (seed, chunkX, chunkY) always yields the identical
    // chunk on every platform, so the streamed world is reproducible and any chunk can
    // be materialized on demand without its neighbours.
    //
    // Each chunk is carved like a stretch of RIVER: a meandering navigable CHANNEL of
    // Empty tiles, cut through solid rock, with occasional wider chambers along it. The
    // rock immediately lining the channel is a thin BANK (Wall, with some bomb-through
    // Brick for shortcuts and power-ups); everything deeper than the bank is VOID — the
    // abyss the channel was cut through, visible behind the rock but never reachable.
    //
    // Connectivity is guaranteed BY CONSTRUCTION without neighbour queries: where the
    // channel crosses each chunk edge is a pure function of the world seed and the
    // SHARED seam identity, so a chunk's crossing always lines up with its neighbour's
    // across every seam. Inside the chunk every crossing is carved to a common hub, so
    // the whole world's channel is one connected component — walkable chunk-to-chunk
    // with no bombing. (Bricks are bomb-through and only ever border the channel, so
    // they never partition it either.)
    Chunk generateChunk(std::uint64_t seed, int chunkX, int chunkY);

    // The difficulty/theme TIER of a chunk, rising with distance from the origin so the
    // world grows tighter, emptier and more fantastic the farther the player travels.
    // This is the single escalation-policy seam: it currently steps in radial rings, but
    // changing the policy (e.g. to a smooth gradient) is a change to this one function
    // plus the tier table in world_gen.cpp. The view reads it to pick a region palette.
    int worldTier(int chunkX, int chunkY);
} // namespace pyrelite
