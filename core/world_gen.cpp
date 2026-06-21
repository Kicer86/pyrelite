
#include "world_gen.h"

#include "rng.h"

namespace pyrelite
{
    namespace
    {
        // Pillar of the indestructible lattice: global even/even cells. Continuous
        // across chunk seams because kChunkSize is even. `% 2 == 0` classifies
        // negative coordinates correctly too (-2 % 2 == 0, -1 % 2 == -1).
        bool isPillar(int globalX, int globalY)
        {
            return globalX % 2 == 0 && globalY % 2 == 0;
        }

        // Every-4th global row, floored so the lane phase is stable across the origin
        // (a plain `% 4` would shift for negative rows).
        bool isCorridorLane(int globalY)
        {
            return ((globalY % 4) + 4) % 4 == 1;
        }

        // A chunk-unique, well-avalanched seed from the world seed and chunk coords,
        // so adjacent chunks look unrelated (a splitmix64 finaliser over a hash mix).
        std::uint64_t chunkSeed(std::uint64_t seed, int chunkX, int chunkY)
        {
            std::uint64_t h = seed;
            h ^= static_cast<std::uint64_t>(static_cast<std::uint32_t>(chunkX)) * 0xD1B54A32D192ED03ULL;
            h ^= static_cast<std::uint64_t>(static_cast<std::uint32_t>(chunkY)) * 0x9E3779B97F4A7C15ULL;
            h = (h ^ (h >> 30)) * 0xBF58476D1CE4E5B9ULL;
            h = (h ^ (h >> 27)) * 0x94D049BB133111EBULL;
            return h ^ (h >> 31);
        }

        // Brick fill density (percent) of the non-pillar cells, per biome.
        int brickPercentFor(Biome biome)
        {
            switch (biome)
            {
            case Biome::Plaza:    return 22;
            case Biome::Thicket:  return 85;
            case Biome::Corridor: return 55;
            case Biome::Rooms:    return 60;
            }
            return 55;
        }

        // How many lattice pillars to keep (percent); open biomes thin them for an
        // airier feel. Removing a pillar only ever opens space, so it can never seal
        // a region — the connectivity guarantee is preserved.
        int pillarKeepPercentFor(Biome biome)
        {
            switch (biome)
            {
            case Biome::Plaza:    return 55;
            case Biome::Thicket:  return 100;
            case Biome::Corridor: return 100;
            case Biome::Rooms:    return 90;
            }
            return 100;
        }
    }

    Chunk generateChunk(std::uint64_t seed, int chunkX, int chunkY)
    {
        Rng rng(chunkSeed(seed, chunkX, chunkY));
        const Biome biome = static_cast<Biome>(rng.below(kBiomeCount));
        Chunk chunk(chunkX, chunkY, biome);

        const int brickPercent = brickPercentFor(biome);
        const int pillarKeep = pillarKeepPercentFor(biome);
        const int originX = chunkX * kChunkSize;
        const int originY = chunkY * kChunkSize;

        for (int ly = 0; ly < kChunkSize; ++ly)
        {
            for (int lx = 0; lx < kChunkSize; ++lx)
            {
                const int gx = originX + lx;
                const int gy = originY + ly;

                if (isPillar(gx, gy))
                {
                    // chance(100) is always true; short-circuit so a non-thinning
                    // biome draws no RNG here (keeps the stream tight, still exact).
                    if (pillarKeep >= 100 || rng.chance(pillarKeep))
                        chunk.set(lx, ly, Tile::Wall);
                    // else thinned -> left Empty (open)
                }
                else if (rng.chance(brickPercent))
                {
                    chunk.set(lx, ly, Tile::Brick);
                }
                // else Empty (the Grid default)
            }
        }

        // Corridor biome: carve clear horizontal lanes so it reads as corridors, not
        // uniform noise. Forcing cells Empty never seals anything.
        if (biome == Biome::Corridor)
        {
            for (int ly = 0; ly < kChunkSize; ++ly)
            {
                if (!isCorridorLane(originY + ly))
                    continue;
                for (int lx = 0; lx < kChunkSize; ++lx)
                    if (!isPillar(originX + lx, originY + ly))
                        chunk.set(lx, ly, Tile::Empty);
            }
        }

        // Rooms biome: carve one fully open chamber (pillars included) amid the
        // bricks. Clearing tiles only opens space, so connectivity is unaffected.
        if (biome == Biome::Rooms)
        {
            const int roomW = 4 + static_cast<int>(rng.below(4)); // 4..7
            const int roomH = 4 + static_cast<int>(rng.below(4));
            const int roomX = static_cast<int>(rng.below(kChunkSize - roomW));
            const int roomY = static_cast<int>(rng.below(kChunkSize - roomH));
            for (int ly = roomY; ly < roomY + roomH; ++ly)
                for (int lx = roomX; lx < roomX + roomW; ++lx)
                    chunk.set(lx, ly, Tile::Empty);
        }

        // Keep the world origin's spawn pocket clear so the player is never boxed in
        // at the start (matches the classic top-left opening).
        if (chunkX == 0 && chunkY == 0)
        {
            chunk.set(1, 1, Tile::Empty);
            chunk.set(2, 1, Tile::Empty);
            chunk.set(1, 2, Tile::Empty);
        }

        return chunk;
    }
} // namespace pyrelite
