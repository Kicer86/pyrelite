
#include "world/world_gen.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include "rng/rng.h"

namespace pyrelite
{
    namespace
    {
        constexpr int kLast = kChunkSize - 1;       // last local index (the border)
        constexpr int kCenter = kChunkSize / 2;

        constexpr int kRingsPerTier = 2;            // chunks per escalation ring
        constexpr int kMaxTier = 4;

        // splitmix64 finaliser — turns a mixed key into a well-avalanched 64-bit value.
        std::uint64_t mix64(std::uint64_t h)
        {
            h = (h ^ (h >> 30)) * 0xBF58476D1CE4E5B9ULL;
            h = (h ^ (h >> 27)) * 0x94D049BB133111EBULL;
            return h ^ (h >> 31);
        }

        // A chunk-unique, well-avalanched seed, so adjacent chunks look unrelated.
        std::uint64_t chunkSeed(std::uint64_t seed, int chunkX, int chunkY)
        {
            std::uint64_t h = seed;
            h ^= static_cast<std::uint64_t>(static_cast<std::uint32_t>(chunkX)) * 0xD1B54A32D192ED03ULL;
            h ^= static_cast<std::uint64_t>(static_cast<std::uint32_t>(chunkY)) * 0x9E3779B97F4A7C15ULL;
            return mix64(h);
        }

        // A value for a SHARED chunk boundary, identical from either side, so the two
        // chunks that meet at a seam agree on where the channel crosses it without ever
        // querying each other. axis 0 = vertical seam at boundary x = bx (opening spans
        // rows); axis 1 = horizontal seam at boundary y = by (opening spans columns).
        std::uint64_t seamValue(std::uint64_t seed, int bx, int by, int axis)
        {
            std::uint64_t h = seed * 0x100000001B3ULL + 0x2545F4914F6CDD1DULL;
            h ^= static_cast<std::uint64_t>(static_cast<std::uint32_t>(bx)) * 0xD1B54A32D192ED03ULL;
            h ^= static_cast<std::uint64_t>(static_cast<std::uint32_t>(by)) * 0x9E3779B97F4A7C15ULL;
            h ^= static_cast<std::uint64_t>(static_cast<std::uint32_t>(axis)) * 0xC2B2AE3D27D4EB4FULL;
            return mix64(h);
        }

        // Where (and how wide) the channel crosses one chunk edge: a centre cell on the
        // edge plus a half-width, kept clear of the corners so a crossing never lands on
        // one. Derived purely from a seamValue, so both sides of the seam agree.
        struct Crossing
        {
            int center;
            int half;
        };

        Crossing crossingFrom(std::uint64_t v)
        {
            constexpr int kMargin = 3;
            constexpr int kSpan = kChunkSize - 2 * kMargin;
            Crossing cr;
            cr.center = kMargin + static_cast<int>(v % static_cast<std::uint64_t>(kSpan));
            cr.half = (v >> 16) % 3 == 0 ? 1 : 0; // ~1/3 of crossings are three wide
            return cr;
        }

        // The decoration knobs for one chunk: corridor width, how deep the rock bank is
        // before it gives way to void, and how much brick/island/chamber to scatter.
        struct StyleParams
        {
            int corridorRadius;
            int bankDepth;
            int brickPct;
            int islandPct;
            int chamberCount;
        };

        // Difficulty/theme by tier: outward chunks get tighter channels, thinner banks
        // (so more void shows through), fewer chambers and sparser brick.
        StyleParams tierBase(int tier)
        {
            switch (tier)
            {
            case 0:  return {1, 3, 32, 10, 2};
            case 1:  return {1, 2, 28, 12, 2};
            case 2:  return {1, 2, 24, 13, 1};
            case 3:  return {0, 1, 20, 15, 1};
            default: return {0, 1, 16, 18, 1};
            }
        }

        // Per-chunk style flavour layered on the tier base — connectivity-neutral: it
        // only nudges decoration density, never the channel skeleton. (Mirrors the enemy
        // archetype switch: a new style is one case here.)
        StyleParams styleFor(Biome biome, int tier)
        {
            StyleParams s = tierBase(tier);
            switch (biome)
            {
            case Biome::Hall:    if (s.chamberCount > 0) --s.chamberCount; break;
            case Biome::Warren:  s.chamberCount += 2; break;
            case Biome::Pillars: s.islandPct += 14; break;
            case Biome::Thicket: s.brickPct += 22; break;
            case Biome::Cavern:  ++s.chamberCount; s.corridorRadius = std::max(s.corridorRadius, 1); break;
            }
            return s;
        }

        int jitter(Rng &rng) { return -2 + static_cast<int>(rng.below(5)); } // [-2, 2]

        // Carve one floor cell. Clamped to the interior, so corridors and chambers can
        // never punch the border ring — the only openings in the ring are the explicit
        // seam crossings, which keeps a chunk's edges aligned with its neighbours'.
        void carveCell(Chunk &chunk, int x, int y)
        {
            if (x >= 1 && x <= kLast - 1 && y >= 1 && y <= kLast - 1)
                chunk.set(x, y, Tile::Empty);
        }

        // Carve a diamond of floor of the given radius (0 = a single cell).
        void carveBlob(Chunk &chunk, int cx, int cy, int radius)
        {
            for (int dy = -radius; dy <= radius; ++dy)
                for (int dx = -radius; dx <= radius; ++dx)
                    if (std::abs(dx) + std::abs(dy) <= radius)
                        carveCell(chunk, cx + dx, cy + dy);
        }

        void carveCrossingV(Chunk &chunk, int edgeX, const Crossing &cr)
        {
            const int inner = edgeX == 0 ? 1 : kLast - 1;
            for (int dy = -cr.half; dy <= cr.half; ++dy)
            {
                const int y = cr.center + dy;
                if (y >= 1 && y <= kLast - 1)
                {
                    chunk.set(edgeX, y, Tile::Empty); // the opening in the ring
                    chunk.set(inner, y, Tile::Empty); // and the cell just inside it
                }
            }
        }

        void carveCrossingH(Chunk &chunk, int edgeY, const Crossing &cr)
        {
            const int inner = edgeY == 0 ? 1 : kLast - 1;
            for (int dx = -cr.half; dx <= cr.half; ++dx)
            {
                const int x = cr.center + dx;
                if (x >= 1 && x <= kLast - 1)
                {
                    chunk.set(x, edgeY, Tile::Empty);
                    chunk.set(x, inner, Tile::Empty);
                }
            }
        }

        // A meandering corridor from (ax, ay) to (bx, by): each step moves strictly
        // toward the target, but the choice of axis when both still differ is random, so
        // the path staircases rather than running straight. Width is occasionally
        // pinched to a single cell, giving tight chokepoints between wider stretches.
        void carveCorridor(Chunk &chunk, Rng &rng, int ax, int ay, int bx, int by, int radius)
        {
            int x = ax;
            int y = ay;
            carveBlob(chunk, x, y, radius);
            while (x != bx || y != by)
            {
                const bool canX = x != bx;
                const bool canY = y != by;
                const bool stepX = canX && (!canY || rng.below(2) == 0);
                if (stepX)
                    x += bx > x ? 1 : -1;
                else
                    y += by > y ? 1 : -1;
                const int eff = (radius > 0 && rng.chance(65)) ? radius : 0;
                carveBlob(chunk, x, y, eff);
            }
        }

        // Turn the solid rock into banks and void by distance from the channel: the cell
        // touching the channel is a bomb-through brick (or wall); deeper-but-shallow rock
        // is wall; anything past the bank is void. The border ring stays solid wall
        // (except its carved crossings), framing the chunk. Because void is always at
        // least bankDepth (>= 1) cells from any floor, it never touches a floor cell, so
        // it is always seen behind rock and never reachable.
        void applyDepth(Chunk &chunk, const StyleParams &sp, Rng &rng)
        {
            constexpr int kCells = kChunkSize * kChunkSize;
            std::array<int, kCells> dist;
            dist.fill(-1);
            std::vector<int> queue;
            queue.reserve(kCells);

            for (int y = 0; y < kChunkSize; ++y)
                for (int x = 0; x < kChunkSize; ++x)
                    if (chunk.at(x, y) == Tile::Empty)
                    {
                        dist[static_cast<std::size_t>(y) * kChunkSize + x] = 0;
                        queue.push_back(y * kChunkSize + x);
                    }

            for (std::size_t head = 0; head < queue.size(); ++head)
            {
                const int idx = queue[head];
                const int x = idx % kChunkSize;
                const int y = idx / kChunkSize;
                const int nb[4][2] = {{x - 1, y}, {x + 1, y}, {x, y - 1}, {x, y + 1}};
                for (const auto &n : nb)
                {
                    if (n[0] < 0 || n[0] >= kChunkSize || n[1] < 0 || n[1] >= kChunkSize)
                        continue;
                    const std::size_t nidx = static_cast<std::size_t>(n[1]) * kChunkSize + n[0];
                    if (dist[nidx] != -1)
                        continue;
                    dist[nidx] = dist[static_cast<std::size_t>(idx)] + 1;
                    queue.push_back(static_cast<int>(nidx));
                }
            }

            for (int y = 0; y < kChunkSize; ++y)
                for (int x = 0; x < kChunkSize; ++x)
                {
                    if (chunk.at(x, y) == Tile::Empty)
                        continue;
                    if (x == 0 || y == 0 || x == kLast || y == kLast)
                        continue; // solid frame
                    const int d = dist[static_cast<std::size_t>(y) * kChunkSize + x];
                    if (d == 1)
                        chunk.set(x, y, rng.chance(sp.brickPct) ? Tile::Brick : Tile::Wall);
                    else if (d <= sp.bankDepth)
                        chunk.set(x, y, Tile::Wall);
                    else
                        chunk.set(x, y, Tile::Void);
                }
        }

        // Sparse bomb-through cover sitting in open floor. A candidate qualifies only if
        // its whole 3x3 neighbourhood is floor: bricking a cell whose eight neighbours
        // are all floor can never partition the channel (the orthogonal neighbours stay
        // connected around it via the corners), so islands never seal a corridor — even
        // where two of them border the same cell. The odd/odd lattice keeps them spaced.
        bool openThreeByThree(const Chunk &chunk, int cx, int cy)
        {
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                    if (chunk.at(cx + dx, cy + dy) != Tile::Empty)
                        return false;
            return true;
        }

        void placeIslands(Chunk &chunk, Rng &rng, const StyleParams &sp)
        {
            for (int y = 3; y <= kLast - 3; ++y)
                for (int x = 3; x <= kLast - 3; ++x)
                {
                    if (x % 2 != 1 || y % 2 != 1)
                        continue;
                    if (!openThreeByThree(chunk, x, y))
                        continue;
                    if (rng.chance(sp.islandPct))
                        chunk.set(x, y, Tile::Brick);
                }
        }
    }

    int worldTier(int chunkX, int chunkY)
    {
        const int ring = std::max(std::abs(chunkX), std::abs(chunkY));
        return std::min(ring / kRingsPerTier, kMaxTier);
    }

    Chunk generateChunk(std::uint64_t seed, int chunkX, int chunkY)
    {
        Rng rng(chunkSeed(seed, chunkX, chunkY));
        const Biome biome = static_cast<Biome>(rng.below(kBiomeCount));
        Chunk chunk(chunkX, chunkY, biome);
        const StyleParams sp = styleFor(biome, worldTier(chunkX, chunkY));

        // 1. Start solid: the rock the channel is cut from.
        for (int y = 0; y < kChunkSize; ++y)
            for (int x = 0; x < kChunkSize; ++x)
                chunk.set(x, y, Tile::Wall);

        // 2. Seam crossings — shared with each neighbour, so the channel lines up across
        //    every seam. A boundary is identified by its larger-side chunk index + axis.
        const Crossing west  = crossingFrom(seamValue(seed, chunkX,     chunkY,     0));
        const Crossing east  = crossingFrom(seamValue(seed, chunkX + 1, chunkY,     0));
        const Crossing north = crossingFrom(seamValue(seed, chunkX,     chunkY,     1));
        const Crossing south = crossingFrom(seamValue(seed, chunkX,     chunkY + 1, 1));
        carveCrossingV(chunk, 0, west);
        carveCrossingV(chunk, kLast, east);
        carveCrossingH(chunk, 0, north);
        carveCrossingH(chunk, kLast, south);

        // 3. Meander every crossing to a common jittered hub: one connected channel.
        const int hubX = std::clamp(kCenter + jitter(rng), 4, kLast - 4);
        const int hubY = std::clamp(kCenter + jitter(rng), 4, kLast - 4);
        carveBlob(chunk, hubX, hubY, std::max(1, sp.corridorRadius));
        carveCorridor(chunk, rng, 1,            west.center,  hubX, hubY, sp.corridorRadius);
        carveCorridor(chunk, rng, kLast - 1,    east.center,  hubX, hubY, sp.corridorRadius);
        carveCorridor(chunk, rng, north.center, 1,            hubX, hubY, sp.corridorRadius);
        carveCorridor(chunk, rng, south.center, kLast - 1,    hubX, hubY, sp.corridorRadius);

        // 4. Occasional wider chambers, each linked back to the channel.
        for (int i = 0; i < sp.chamberCount; ++i)
        {
            const int chx = std::clamp(3 + static_cast<int>(rng.below(kChunkSize - 6)), 3, kLast - 3);
            const int chy = std::clamp(3 + static_cast<int>(rng.below(kChunkSize - 6)), 3, kLast - 3);
            carveBlob(chunk, chx, chy, 1 + static_cast<int>(rng.below(2)));
            carveCorridor(chunk, rng, chx, chy, hubX, hubY, sp.corridorRadius);
        }

        // 5. Origin spawn pocket: a guaranteed-clear corner linked to the channel, so
        //    the first run starts in the open and can walk out without bombing.
        if (chunkX == 0 && chunkY == 0)
        {
            chunk.set(1, 1, Tile::Empty);
            chunk.set(2, 1, Tile::Empty);
            chunk.set(1, 2, Tile::Empty);
            carveCorridor(chunk, rng, 2, 1, hubX, hubY, 0);
        }

        // 6. Carve banks and void from the distance to the channel.
        applyDepth(chunk, sp, rng);

        // 7. Bomb-through island cover in the open stretches.
        placeIslands(chunk, rng, sp);

        return chunk;
    }
} // namespace pyrelite
