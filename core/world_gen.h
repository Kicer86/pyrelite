
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
    // Every chunk lays the same GLOBAL even/even pillar lattice (continuous across
    // seams) and fills the rest, per its biome, with destructible bricks. No biome
    // ever places an indestructible wall off the lattice (biomes only ADD bricks or
    // REMOVE pillars), so the odd corridors stay connected across the whole world —
    // it is always navigable (by bombing through bricks) by construction.
    Chunk generateChunk(std::uint64_t seed, int chunkX, int chunkY);
} // namespace pyrelite
