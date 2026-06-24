
#include "world/world_gen.h"

#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <set>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "world/chunk.h"
#include "world/zone_cache.h"
#include "grid/grid.h"

using namespace pyrelite;

namespace
{
    // kZoneChunks / kZoneSize are the public zone dimensions (world/zone.h).
    constexpr int kSpawnInOriginZone = kZoneSize / 2 + 1;

    // Fold a chunk's tiles into a 64-bit signature (FNV-1a, order-sensitive) so a
    // generated layout can be pinned exactly across builds and platforms — the
    // "stable starting seed" regression guard.
    std::uint64_t signature(const Chunk &chunk)
    {
        std::uint64_t h = 14695981039346656037ULL; // FNV-1a 64-bit offset basis
        for (int ly = 0; ly < kChunkSize; ++ly)
            for (int lx = 0; lx < kChunkSize; ++lx)
            {
                h ^= static_cast<std::uint64_t>(chunk.at(lx, ly));
                h *= 1099511628211ULL;
            }
        return h;
    }

    // A rectangular block of chunks flattened into one tile field, addressed in
    // global coordinates, for whole-region connectivity checks across seams.
    struct Region
    {
        int width = 0;
        int height = 0;
        std::vector<Tile> tiles;
    };

    Region materialize(std::uint64_t seed, int cx0, int cy0, int cxCount, int cyCount)
    {
        Region r;
        r.width = cxCount * kChunkSize;
        r.height = cyCount * kChunkSize;
        r.tiles.assign(static_cast<std::size_t>(r.width) * r.height, Tile::Empty);
        for (int cy = 0; cy < cyCount; ++cy)
            for (int cx = 0; cx < cxCount; ++cx)
            {
                const Chunk chunk = generateChunk(seed, cx0 + cx, cy0 + cy);
                for (int ly = 0; ly < kChunkSize; ++ly)
                    for (int lx = 0; lx < kChunkSize; ++lx)
                    {
                        const int ix = cx * kChunkSize + lx;
                        const int iy = cy * kChunkSize + ly;
                        r.tiles[static_cast<std::size_t>(iy) * r.width + ix] = chunk.at(lx, ly);
                    }
            }
        return r;
    }

    template <typename Pred>
    int countIf(const Region &r, Pred pred)
    {
        int n = 0;
        for (Tile t : r.tiles)
            if (pred(t))
                ++n;
        return n;
    }

    template <typename Pred>
    int firstWhere(const Region &r, Pred pred)
    {
        for (std::size_t i = 0; i < r.tiles.size(); ++i)
            if (pred(r.tiles[i]))
                return static_cast<int>(i);
        return -1;
    }

    // Flood from `start`, crossing only cells that pass `passable`. Returns the count seen.
    template <typename Passable>
    int floodCount(const Region &r, int start, Passable passable)
    {
        std::vector<char> seen(r.tiles.size(), 0);
        if (start < 0 || !passable(r.tiles[start]))
            return 0;

        int reached = 0;
        std::vector<int> stack{start};
        while (!stack.empty())
        {
            const int idx = stack.back();
            stack.pop_back();
            if (seen[idx] || !passable(r.tiles[idx]))
                continue;
            seen[idx] = 1;
            ++reached;

            const int lx = idx % r.width;
            const int ly = idx / r.width;
            if (lx > 0)            stack.push_back(idx - 1);
            if (lx < r.width - 1)  stack.push_back(idx + 1);
            if (ly > 0)            stack.push_back(idx - r.width);
            if (ly < r.height - 1) stack.push_back(idx + r.width);
        }
        return reached;
    }

    bool isEmpty(Tile t) { return t == Tile::Empty; }
    bool notSolid(Tile t) { return !isSolid(t); }

    int zoneMinChunk(int zoneCoord)
    {
        // Generation zones are centred around the world origin: zone zero contains
        // chunks [-2, 1], so the spawn does not sit next to a generation boundary.
        return zoneCoord * kZoneChunks - kZoneChunks / 2;
    }

    int emptyCrossingsOnVerticalZoneEdge(std::uint64_t seed, int westZoneX, int zoneY)
    {
        const int westChunkX = zoneMinChunk(westZoneX) + kZoneChunks - 1;
        const int eastChunkX = westChunkX + 1;
        int crossings = 0;
        for (int chunkOffset = 0; chunkOffset < kZoneChunks; ++chunkOffset)
        {
            const int chunkY = zoneMinChunk(zoneY) + chunkOffset;
            const Chunk west = generateChunk(seed, westChunkX, chunkY);
            const Chunk east = generateChunk(seed, eastChunkX, chunkY);
            for (int y = 0; y < kChunkSize; ++y)
                if (west.at(kChunkSize - 1, y) == Tile::Empty
                    && east.at(0, y) == Tile::Empty)
                    ++crossings;
        }
        return crossings;
    }

    std::vector<std::pair<int, int>> squarePerimeter(int centerX, int centerY, int radius)
    {
        std::vector<std::pair<int, int>> cells;
        cells.reserve(static_cast<std::size_t>(radius) * 8);
        for (int x = centerX - radius; x < centerX + radius; ++x)
            cells.emplace_back(x, centerY - radius);
        for (int y = centerY - radius; y < centerY + radius; ++y)
            cells.emplace_back(centerX + radius, y);
        for (int x = centerX + radius; x > centerX - radius; --x)
            cells.emplace_back(x, centerY + radius);
        for (int y = centerY + radius; y > centerY - radius; --y)
            cells.emplace_back(centerX - radius, y);
        return cells;
    }
}

TEST(WorldGenTest, SameSeedAndCoordsSameChunk)
{
    const std::pair<int, int> coords[] = {{0, 0}, {3, 5}, {-2, 4}, {-7, -9}};
    for (auto [cx, cy] : coords)
    {
        const Chunk a = generateChunk(42, cx, cy);
        const Chunk b = generateChunk(42, cx, cy);
        EXPECT_EQ(a.biome(), b.biome()) << "biome at " << cx << "," << cy;
        for (int ly = 0; ly < kChunkSize; ++ly)
            for (int lx = 0; lx < kChunkSize; ++lx)
                EXPECT_EQ(a.at(lx, ly), b.at(lx, ly)) << "tile " << lx << "," << ly;
    }
}

TEST(WorldGenTest, NeighbouringChunksDiffer)
{
    // Avalanched per-chunk seeds: adjacent chunks must not be carbon copies.
    const std::set<std::uint64_t> sigs = {
        signature(generateChunk(1, 0, 0)),
        signature(generateChunk(1, 1, 0)),
        signature(generateChunk(1, 0, 1)),
        signature(generateChunk(1, 1, 1)),
    };
    EXPECT_GT(sigs.size(), 1u);
}

TEST(WorldGenTest, GenerationOrderDoesNotAffectChunks)
{
    const std::pair<int, int> coords[] = {
        {0, 0}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}, {4, -3}, {-5, 2},
    };
    std::vector<std::uint64_t> expected;
    for (const auto &[x, y] : coords)
        expected.push_back(signature(generateChunk(73, x, y)));

    for (auto it = std::rbegin(coords); it != std::rend(coords); ++it)
        (void) generateChunk(73, it->first, it->second);

    for (std::size_t i = 0; i < std::size(coords); ++i)
        EXPECT_EQ(signature(generateChunk(73, coords[i].first, coords[i].second)), expected[i]);
}

TEST(WorldGenTest, AllStylesAppearAcrossChunks)
{
    std::set<Biome> styles;
    for (int cy = -24; cy < 24; ++cy)
        for (int cx = -24; cx < 24; ++cx)
            styles.insert(generateChunk(1, cx, cy).biome());
    EXPECT_EQ(styles.size(), static_cast<std::size_t>(kBiomeCount));
}

TEST(WorldGenTest, ChannelIsOneConnectedComponentWithoutBombing)
{
    // The whole region's floor (Empty) is a single connected component: flooding over
    // Empty ONLY (no bombing) from any one floor cell must reach every floor cell, on
    // any seed. This is the by-construction "walkable chunk-to-chunk" guarantee that
    // replaces the old fixed-doorway spine.
    for (std::uint64_t seed = 1; seed <= 6; ++seed)
    {
        const Region r = materialize(seed, -6, -6, 12, 12);
        const int reached = floodCount(r, firstWhere(r, isEmpty), isEmpty);
        EXPECT_EQ(reached, countIf(r, isEmpty)) << "seed " << seed;
    }
}

TEST(WorldGenTest, PlayableAreaIsReachableByBombing)
{
    // The playable area (floor + bomb-through brick, i.e. everything not solid) is one
    // connected component: with bombing, the player can reach every non-solid cell. No
    // brick is ever sealed off behind solid rock/void.
    for (std::uint64_t seed = 1; seed <= 6; ++seed)
    {
        const Region r = materialize(seed, -6, -6, 12, 12);
        const int reached = floodCount(r, firstWhere(r, notSolid), notSolid);
        EXPECT_EQ(reached, countIf(r, notSolid)) << "seed " << seed;
    }
}

TEST(WorldGenTest, ZoneEdgesAreSelective)
{
    // The old generator opened every chunk edge, forcing each chunk into the same
    // four-way cross. A zone graph must contain both connected and closed boundaries.
    bool foundOpen = false;
    bool foundClosed = false;
    for (int zy = -3; zy <= 3; ++zy)
        for (int zx = -3; zx < 3; ++zx)
        {
            const int crossings = emptyCrossingsOnVerticalZoneEdge(7, zx, zy);
            foundOpen |= crossings > 0;
            foundClosed |= crossings == 0;
        }
    EXPECT_TRUE(foundOpen);
    EXPECT_TRUE(foundClosed);
}

TEST(WorldGenTest, SharedZoneCrossingsAlignExactly)
{
    // Active portals are derived from the shared boundary identity. Both independently
    // generated zones therefore carve exactly the same rows, while a closed edge stays
    // closed on both sides.
    for (int zy = -2; zy <= 2; ++zy)
        for (int zx = -2; zx < 2; ++zx)
            for (int chunkOffset = 0; chunkOffset < kZoneChunks; ++chunkOffset)
            {
                const int westChunkX = zoneMinChunk(zx) + kZoneChunks - 1;
                const int eastChunkX = westChunkX + 1;
                const int chunkY = zoneMinChunk(zy) + chunkOffset;
                const Chunk west = generateChunk(7, westChunkX, chunkY);
                const Chunk east = generateChunk(7, eastChunkX, chunkY);
                for (int y = 0; y < kChunkSize; ++y)
                    EXPECT_EQ(west.at(kChunkSize - 1, y) == Tile::Empty,
                              east.at(0, y) == Tile::Empty)
                        << "zone edge " << zx << "," << zy << " row "
                        << chunkOffset * kChunkSize + y;
            }

    for (int zy = -2; zy < 2; ++zy)
        for (int zx = -2; zx <= 2; ++zx)
            for (int chunkOffset = 0; chunkOffset < kZoneChunks; ++chunkOffset)
            {
                const int northChunkY = zoneMinChunk(zy) + kZoneChunks - 1;
                const int southChunkY = northChunkY + 1;
                const int chunkX = zoneMinChunk(zx) + chunkOffset;
                const Chunk north = generateChunk(7, chunkX, northChunkY);
                const Chunk south = generateChunk(7, chunkX, southChunkY);
                for (int x = 0; x < kChunkSize; ++x)
                    EXPECT_EQ(north.at(x, kChunkSize - 1) == Tile::Empty,
                              south.at(x, 0) == Tile::Empty)
                        << "zone edge " << zx << "," << zy << " column "
                        << chunkOffset * kChunkSize + x;
            }
}

TEST(WorldGenTest, ChunkEdgesInsideZoneAreNotRockFrames)
{
    // Chunks are storage slices only. Their internal edges must not reintroduce the
    // conspicuous Wall frame that made the old world look like a square grid.
    for (std::uint64_t seed = 1; seed <= 4; ++seed)
    {
        const Region zone = materialize(seed, zoneMinChunk(0), zoneMinChunk(0),
                                        kZoneChunks, kZoneChunks);
        int wallPairs = 0;
        int pairs = 0;
        for (int seam = 1; seam < kZoneChunks; ++seam)
        {
            const int edge = seam * kChunkSize;
            for (int i = 0; i < zone.width; ++i)
            {
                const auto verticalLeft = static_cast<std::size_t>(i) * zone.width + edge - 1;
                const auto verticalRight = verticalLeft + 1;
                const auto horizontalTop = static_cast<std::size_t>(edge - 1) * zone.width + i;
                const auto horizontalBottom = horizontalTop + zone.width;
                wallPairs += zone.tiles[verticalLeft] == Tile::Wall
                    && zone.tiles[verticalRight] == Tile::Wall;
                wallPairs += zone.tiles[horizontalTop] == Tile::Wall
                    && zone.tiles[horizontalBottom] == Tile::Wall;
                pairs += 2;
            }
        }
        EXPECT_LT(wallPairs, pairs * 3 / 4) << "seed " << seed;
    }
}

TEST(WorldGenTest, CaveNetworkDoesNotCollapseIntoAnOpenField)
{
    for (std::uint64_t seed = 1; seed <= 12; ++seed)
    {
        const Region zone = materialize(seed, zoneMinChunk(0), zoneMinChunk(0),
                                        kZoneChunks, kZoneChunks);
        const int floor = countIf(zone, isEmpty);
        EXPECT_GT(floor * 100, static_cast<int>(zone.tiles.size()) * 10) << "seed " << seed;
        EXPECT_LT(floor * 100, static_cast<int>(zone.tiles.size()) * 42) << "seed " << seed;
    }
}

TEST(WorldGenTest, ZonesContainRoomyArenaChambers)
{
    // Every zone is anchored by one or two large arenas. Whatever brick pattern packs a
    // given arena, the chamber itself is a roomy pocket of rock-free space far wider than
    // any corridor: it admits an inscribed disc of non-Void tiles that the narrow
    // passages and their thin banks never could. Checked on a non-origin zone so the
    // sealed spawn chamber cannot stand in for an arena.
    constexpr int kArenaDiscRadius = 7;
    auto hasRoomyChamber = [](const Region &zone)
    {
        for (int cy = 0; cy < zone.height; ++cy)
            for (int cx = 0; cx < zone.width; ++cx)
            {
                bool clear = true;
                for (int dy = -kArenaDiscRadius; dy <= kArenaDiscRadius && clear; ++dy)
                    for (int dx = -kArenaDiscRadius; dx <= kArenaDiscRadius && clear; ++dx)
                    {
                        if (dx * dx + dy * dy > kArenaDiscRadius * kArenaDiscRadius)
                            continue;
                        const int x = cx + dx;
                        const int y = cy + dy;
                        if (x < 0 || y < 0 || x >= zone.width || y >= zone.height
                            || zone.tiles[static_cast<std::size_t>(y) * zone.width + x]
                                == Tile::Void)
                            clear = false;
                    }
                if (clear)
                    return true;
            }
        return false;
    };

    for (std::uint64_t seed = 1; seed <= 8; ++seed)
    {
        const Region zone = materialize(seed, zoneMinChunk(2), zoneMinChunk(0),
                                        kZoneChunks, kZoneChunks);
        EXPECT_TRUE(hasRoomyChamber(zone)) << "seed " << seed;
    }
}

TEST(WorldGenTest, VoidNeverTouchesFloor)
{
    // The abyss is always seen behind rock: no floor cell is ever orthogonally adjacent
    // to a Void cell, so Void can never be walked into, bombed into, or pathed through.
    // Checked across several seeds and a region spanning multiple zone seams, where an
    // independently-banked neighbour could otherwise expose void beside a portal.
    for (std::uint64_t seed = 1; seed <= 8; ++seed)
    {
        const Region r = materialize(seed, -3, -3, 6, 6);
        for (int y = 0; y < r.height; ++y)
            for (int x = 0; x < r.width; ++x)
            {
                const std::size_t idx = static_cast<std::size_t>(y) * r.width + x;
                if (r.tiles[idx] != Tile::Empty)
                    continue;
                const std::pair<int, int> nb[] = {
                    {x - 1, y}, {x + 1, y}, {x, y - 1}, {x, y + 1}};
                for (auto [nx, ny] : nb)
                {
                    if (nx < 0 || nx >= r.width || ny < 0 || ny >= r.height)
                        continue;
                    EXPECT_NE(r.tiles[static_cast<std::size_t>(ny) * r.width + nx], Tile::Void)
                        << "seed " << seed << " floor " << x << "," << y << " touches void";
                }
            }
    }
}

TEST(WorldGenTest, SpawnPocketIsClear)
{
    for (std::uint64_t seed = 1; seed <= 8; ++seed)
    {
        const Chunk origin = generateChunk(seed, 0, 0);
        EXPECT_EQ(origin.at(1, 1), Tile::Empty) << "seed " << seed;
        EXPECT_EQ(origin.at(2, 1), Tile::Empty) << "seed " << seed;
        EXPECT_EQ(origin.at(1, 2), Tile::Empty) << "seed " << seed;
    }
}

TEST(WorldGenTest, SpawnChamberHasOneExitInVariedDirections)
{
    constexpr int kExitRingRadius = 9;
    const auto perimeter = squarePerimeter(kSpawnInOriginZone, kSpawnInOriginZone,
                                           kExitRingRadius);
    bool foundDiagonalExit = false;
    for (std::uint64_t seed = 1; seed <= 32; ++seed)
    {
        const Region zone = materialize(seed, zoneMinChunk(0), zoneMinChunk(0),
                                        kZoneChunks, kZoneChunks);
        std::vector<bool> open;
        open.reserve(perimeter.size());
        int openCount = 0;
        int sumDx = 0;
        int sumDy = 0;
        for (const auto &[x, y] : perimeter)
        {
            const Tile tile = zone.tiles[static_cast<std::size_t>(y) * zone.width + x];
            const bool isOpen = tile == Tile::Empty;
            open.push_back(isOpen);
            if (isOpen)
            {
                ++openCount;
                sumDx += x - kSpawnInOriginZone;
                sumDy += y - kSpawnInOriginZone;
            }
            else
                EXPECT_EQ(tile, Tile::Wall) << "seed " << seed << " at " << x << "," << y;
        }

        int runs = 0;
        for (std::size_t i = 0; i < open.size(); ++i)
            if (open[i] && !open[(i + open.size() - 1) % open.size()])
                ++runs;

        EXPECT_EQ(runs, 1) << "seed " << seed;
        EXPECT_GE(openCount, 1) << "seed " << seed;
        EXPECT_LE(openCount, 8) << "seed " << seed;
        foundDiagonalExit |= openCount > 0
            && std::abs(sumDx) * 2 > openCount * kExitRingRadius
            && std::abs(sumDy) * 2 > openCount * kExitRingRadius;

        const int spawnIndex = kSpawnInOriginZone * zone.width + kSpawnInOriginZone;
        EXPECT_EQ(floodCount(zone, spawnIndex, isEmpty), countIf(zone, isEmpty))
            << "seed " << seed;
    }
    EXPECT_TRUE(foundDiagonalExit);
}

TEST(WorldGenTest, OnlyKnownTiles)
{
    const std::pair<int, int> coords[] = {{0, 0}, {2, 3}, {-5, 1}};
    for (auto [cx, cy] : coords)
    {
        const Chunk chunk = generateChunk(7, cx, cy);
        for (int ly = 0; ly < kChunkSize; ++ly)
            for (int lx = 0; lx < kChunkSize; ++lx)
            {
                const Tile t = chunk.at(lx, ly);
                EXPECT_TRUE(t == Tile::Empty || t == Tile::Wall || t == Tile::Brick
                            || t == Tile::Void)
                    << "tile " << lx << "," << ly << " at chunk " << cx << "," << cy;
            }
    }
}

TEST(WorldGenTest, TiersEscalateWithDistanceFromOrigin)
{
    // Difficulty/theme rises with distance from spawn and never falls as you go outward.
    EXPECT_EQ(worldTier(0, 0), 0);
    int prev = 0;
    for (int ring = 0; ring <= 16; ++ring)
    {
        const int tier = worldTier(ring, 0);
        EXPECT_GE(tier, prev) << "ring " << ring;
        prev = tier;
    }
    EXPECT_GT(worldTier(16, 0), worldTier(0, 0));
    EXPECT_EQ(worldTier(5, -9), worldTier(9, 0)); // tier follows the zone's Chebyshev ring

    std::set<int> tiers;
    for (int c = 0; c <= 16; ++c)
        tiers.insert(worldTier(c, c));
    EXPECT_GE(tiers.size(), 3u);
}

TEST(WorldGenTest, GoldenSeedsAreStable)
{
    // Pinned signatures for a handful of fixed seeds: the deterministic "stable
    // starting worlds" guarantee. If generation logic changes intentionally, these
    // are expected to change; an UNINTENDED change is a determinism regression.
    struct Golden { std::uint64_t seed; int cx; int cy; std::uint64_t sig; };
    const Golden golden[] = {
        {1, 0, 0, 707348581913096066ULL},
        {2, 0, 0, 15987644042394766286ULL},
        {3, 0, 0, 16643534575326783970ULL},
        {1, 1, 0, 15468943342674645611ULL},
        {1, -1, -1, 3928926359074671741ULL},
    };
    for (const Golden &g : golden)
        EXPECT_EQ(signature(generateChunk(g.seed, g.cx, g.cy)), g.sig)
            << "seed " << g.seed << " chunk " << g.cx << "," << g.cy;
}

TEST(WorldGenTest, ZoneCacheMatchesDirectGeneration)
{
    // A ZoneCache is a pure memo: a chunk sliced from it must equal generateChunk for
    // every chunk, in any access order and across repeats, so callers can cache zones
    // without changing the world. Covers four chunks of one zone, a cross-zone jump and
    // a repeat (cache hit) plus an eviction past capacity.
    ZoneCache cache(2);
    const std::pair<int, int> coords[] = {
        {0, 0}, {1, 0}, {2, 0}, {3, 0}, {-1, 0}, {0, 1}, {7, -5}, {7, -5}, {-9, 9},
    };
    for (auto [cx, cy] : coords)
        EXPECT_EQ(signature(cache.chunk(9, cx, cy)), signature(generateChunk(9, cx, cy)))
            << "chunk " << cx << "," << cy;
}
