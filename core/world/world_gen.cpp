
#include "world/world_gen.h"

#include "rng/rng.h"

namespace pyrelite
{
    namespace
    {
        constexpr int kDoor = kChunkSize / 2;     // edge-midpoint doorway position
        constexpr int kLast = kChunkSize - 1;     // last local index (the far border)

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

        bool isBorder(int x, int y) { return x == 0 || y == 0 || x == kLast || y == kLast; }
        bool isCorner(int x, int y) { return (x == 0 || x == kLast) && (y == 0 || y == kLast); }

        // The four edge-midpoint cells: the doorway gaps. Fixed positions, so a chunk's
        // doorway always faces its neighbour's — seams connect by construction.
        bool isDoor(int x, int y)
        {
            return (x == kDoor && (y == 0 || y == kLast)) || (y == kDoor && (x == 0 || x == kLast));
        }

        // Border cells immediately flanking a doorway — kept as stone so every door has
        // a solid frame regardless of the random stretch material.
        bool isDoorJamb(int x, int y)
        {
            if (x == 0 || x == kLast)
                return y == kDoor - 1 || y == kDoor + 1;
            if (y == 0 || y == kLast)
                return x == kDoor - 1 || x == kDoor + 1;
            return false;
        }

        bool onSpine(int x, int y) { return x == kDoor || y == kDoor; }

        // A pillar may only sit on an odd/odd cell of the inner field. Odd/odd cells are
        // never 8-adjacent to one another, and the inner field keeps them clear of the
        // border, so isolated indestructible pillars can NEVER ring-in a pocket — the
        // flood-fill connectivity guarantee holds whatever the random draw. (This is the
        // deliberate opposite of the old every-even/even lattice: a sparse, jittered
        // subset, not a wall on one cell in four.)
        bool isPillarSlot(int x, int y)
        {
            return x >= 3 && x <= kChunkSize - 3 && y >= 3 && y <= kChunkSize - 3
                && x % 2 == 1 && y % 2 == 1;
        }

        // 1. The chamber wall: a hybrid ring of stone anchors (corners + door frames) and
        //    brick stretches (bomb-through, so a player can carve extra shortcuts between
        //    rooms). stoneBias varies the stone/brick mix per chunk.
        void layPerimeter(Chunk &chunk, Rng &rng, int stoneBias)
        {
            for (int y = 0; y < kChunkSize; ++y)
                for (int x = 0; x < kChunkSize; ++x)
                {
                    if (!isBorder(x, y) || isDoor(x, y))
                        continue;
                    const bool anchor = isCorner(x, y) || isDoorJamb(x, y);
                    chunk.set(x, y, anchor || rng.chance(stoneBias) ? Tile::Wall : Tile::Brick);
                }
        }

        // 2. The four doorways plus the cell just inside each (the gateway): always Empty,
        //    so neighbouring chambers connect without bombing.
        void carveDoorways(Chunk &chunk)
        {
            chunk.set(kDoor, 0, Tile::Empty);     chunk.set(kDoor, 1, Tile::Empty);
            chunk.set(kDoor, kLast, Tile::Empty); chunk.set(kDoor, kLast - 1, Tile::Empty);
            chunk.set(0, kDoor, Tile::Empty);     chunk.set(1, kDoor, Tile::Empty);
            chunk.set(kLast, kDoor, Tile::Empty); chunk.set(kLast - 1, kDoor, Tile::Empty);
        }

        // 3. The connectivity spine: an Empty cross joining all four doorways through the
        //    centre. This is the navigability backbone — every chamber, whatever its kind,
        //    is crossable without bombing, and the spines of adjacent chunks meet at the
        //    doorways into one global lane network.
        void carveSpine(Chunk &chunk)
        {
            for (int i = 1; i < kLast; ++i)
            {
                chunk.set(i, kDoor, Tile::Empty);
                chunk.set(kDoor, i, Tile::Empty);
            }
        }

        // A Rooms chamber subdivides: 1-2 internal brick dividers, each with a guaranteed
        // gap where it crosses the spine, so the sub-rooms stay reachable. Brick (not
        // stone) on purpose — bomb-through, and it can never seal the flood.
        void carveSubRooms(Chunk &chunk, Rng &rng)
        {
            const int walls = 1 + static_cast<int>(rng.below(2));
            for (int i = 0; i < walls; ++i)
            {
                if (rng.below(2) == 0)
                {
                    int wx = 3 + static_cast<int>(rng.below(kChunkSize - 6));
                    if (wx == kDoor)
                        ++wx;
                    for (int y = 1; y < kLast; ++y)
                        if (y != kDoor)
                            chunk.set(wx, y, Tile::Brick);
                }
                else
                {
                    int wy = 3 + static_cast<int>(rng.below(kChunkSize - 6));
                    if (wy == kDoor)
                        ++wy;
                    for (int x = 1; x < kLast; ++x)
                        if (x != kDoor)
                            chunk.set(x, wy, Tile::Brick);
                }
            }
        }

        // 4. The interior cover, per chamber kind — the variety axis. Stone only ever
        //    lands on a pillar slot (isolated by construction); bricks fill the rest. The
        //    spine and any already-placed divider stay untouched.
        void fillInterior(Chunk &chunk, Rng &rng, Biome biome)
        {
            int stonePct = 0;
            int brickBase = 0;
            int brickJitter = 0;
            bool subdivide = false;
            switch (biome)
            {
            case Biome::Hall:    stonePct = 45; brickBase = 12; brickJitter = 12; break;
            case Biome::Rooms:   stonePct = 30; brickBase = 25; brickJitter = 14; subdivide = true; break;
            case Biome::Pillars: stonePct = 85; brickBase = 18; brickJitter = 14; break;
            case Biome::Thicket: stonePct = 25; brickBase = 60; brickJitter = 18; break;
            case Biome::Plaza:   stonePct = 10; brickBase = 6;  brickJitter = 10; break;
            }
            const int brickPct = brickBase + static_cast<int>(rng.below(brickJitter + 1));

            if (subdivide)
                carveSubRooms(chunk, rng);

            for (int y = 1; y < kLast; ++y)
                for (int x = 1; x < kLast; ++x)
                {
                    if (onSpine(x, y) || chunk.at(x, y) != Tile::Empty)
                        continue;
                    if (isPillarSlot(x, y) && rng.chance(stonePct))
                        chunk.set(x, y, Tile::Wall);
                    else if (rng.chance(brickPct))
                        chunk.set(x, y, Tile::Brick);
                }
        }

        // 5. The origin spawn pocket: a guaranteed-clear corner linked up to the spine,
        //    so the very first run starts in the open and can walk out without bombing.
        void carveSpawnPocket(Chunk &chunk)
        {
            chunk.set(2, 1, Tile::Empty);
            for (int y = 1; y <= kDoor; ++y)
                chunk.set(1, y, Tile::Empty); // column from the corner down to the spine
        }
    }

    Chunk generateChunk(std::uint64_t seed, int chunkX, int chunkY)
    {
        Rng rng(chunkSeed(seed, chunkX, chunkY));
        const Biome biome = static_cast<Biome>(rng.below(kBiomeCount));
        Chunk chunk(chunkX, chunkY, biome);

        const int stoneBias = 30 + static_cast<int>(rng.below(40)); // 30..69% stone on stretches
        layPerimeter(chunk, rng, stoneBias);
        carveDoorways(chunk);
        carveSpine(chunk);
        fillInterior(chunk, rng, biome);

        if (chunkX == 0 && chunkY == 0)
            carveSpawnPocket(chunk);

        return chunk;
    }
} // namespace pyrelite
