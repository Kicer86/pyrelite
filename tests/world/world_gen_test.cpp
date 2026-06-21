
#include "world/world_gen.h"

#include <cstdint>
#include <set>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "world/chunk.h"
#include "grid/grid.h"

using namespace pyrelite;

namespace
{
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

TEST(WorldGenTest, AllStylesAppearAcrossChunks)
{
    std::set<Biome> styles;
    for (int cy = 0; cy < 12; ++cy)
        for (int cx = 0; cx < 12; ++cx)
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
        const Region r = materialize(seed, -1, -1, 3, 3);
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
        const Region r = materialize(seed, -1, -1, 3, 3);
        const int reached = floodCount(r, firstWhere(r, notSolid), notSolid);
        EXPECT_EQ(reached, countIf(r, notSolid)) << "seed " << seed;
    }
}

TEST(WorldGenTest, SeamCrossingsAlignAcrossNeighbours)
{
    // A chunk's channel openings sit at cells shared with the neighbour's, because the
    // crossing is a pure function of the SHARED seam identity. So a floor opening on one
    // chunk's edge always faces a floor opening on the adjoining edge — connectivity by
    // construction, with no neighbour queries.
    const std::pair<int, int> coords[] = {{0, 0}, {2, 3}, {-4, 1}, {-1, -1}};
    for (auto [cx, cy] : coords)
    {
        const Chunk c = generateChunk(7, cx, cy);
        const Chunk east = generateChunk(7, cx + 1, cy);
        const Chunk south = generateChunk(7, cx, cy + 1);
        for (int i = 0; i < kChunkSize; ++i)
        {
            EXPECT_EQ(c.at(kChunkSize - 1, i) == Tile::Empty, east.at(0, i) == Tile::Empty)
                << "east seam row " << i << " at " << cx << "," << cy;
            EXPECT_EQ(c.at(i, kChunkSize - 1) == Tile::Empty, south.at(i, 0) == Tile::Empty)
                << "south seam col " << i << " at " << cx << "," << cy;
        }
    }
}

TEST(WorldGenTest, VoidNeverTouchesFloor)
{
    // The abyss is always seen behind rock: no floor cell is ever orthogonally adjacent
    // to a Void cell, so Void can never be walked into, bombed into, or pathed through.
    const Region r = materialize(3, -1, -1, 3, 3);
    for (int y = 0; y < r.height; ++y)
        for (int x = 0; x < r.width; ++x)
        {
            const std::size_t idx = static_cast<std::size_t>(y) * r.width + x;
            if (r.tiles[idx] != Tile::Empty)
                continue;
            const std::pair<int, int> nb[] = {{x - 1, y}, {x + 1, y}, {x, y - 1}, {x, y + 1}};
            for (auto [nx, ny] : nb)
            {
                if (nx < 0 || nx >= r.width || ny < 0 || ny >= r.height)
                    continue;
                EXPECT_NE(r.tiles[static_cast<std::size_t>(ny) * r.width + nx], Tile::Void)
                    << "floor " << x << "," << y << " touches void";
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
    EXPECT_EQ(worldTier(5, -9), worldTier(9, 0)); // tier follows Chebyshev ring

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
        {1, 0, 0, 13931823529882201580ULL},
        {2, 0, 0, 4720691713657001490ULL},
        {3, 0, 0, 15593927046287269645ULL},
        {1, 1, 0, 4257396582767645368ULL},
        {1, -1, -1, 7608924436878331803ULL},
    };
    for (const Golden &g : golden)
        EXPECT_EQ(signature(generateChunk(g.seed, g.cx, g.cy)), g.sig)
            << "seed " << g.seed << " chunk " << g.cx << "," << g.cy;
}
