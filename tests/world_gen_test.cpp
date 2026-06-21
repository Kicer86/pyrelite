
#include "world_gen.h"

#include <cstdint>
#include <set>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "chunk.h"
#include "grid.h"

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
        int baseX = 0;
        int baseY = 0;
        int width = 0;
        int height = 0;
        std::vector<Tile> tiles;

        Tile local(int lx, int ly) const
        {
            return tiles[static_cast<std::size_t>(ly) * width + lx];
        }
    };

    Region materialize(std::uint64_t seed, int cx0, int cy0, int cxCount, int cyCount)
    {
        Region r;
        r.baseX = cx0 * kChunkSize;
        r.baseY = cy0 * kChunkSize;
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

    int countNonWall(const Region &r)
    {
        int n = 0;
        for (Tile t : r.tiles)
            if (t != Tile::Wall)
                ++n;
        return n;
    }

    // Flood from `start`, crossing only cells that pass `passable`. Returns the seen mask.
    template <typename Passable>
    std::vector<char> flood(const Region &r, int start, Passable passable)
    {
        std::vector<char> seen(r.tiles.size(), 0);
        if (start < 0 || !passable(r.tiles[start]))
            return seen;

        std::vector<int> stack{start};
        while (!stack.empty())
        {
            const int idx = stack.back();
            stack.pop_back();
            if (seen[idx] || !passable(r.tiles[idx]))
                continue;
            seen[idx] = 1;

            const int lx = idx % r.width;
            const int ly = idx / r.width;
            if (lx > 0)            stack.push_back(idx - 1);
            if (lx < r.width - 1)  stack.push_back(idx + 1);
            if (ly > 0)            stack.push_back(idx - r.width);
            if (ly < r.height - 1) stack.push_back(idx + r.width);
        }
        return seen;
    }

    int firstNonWall(const Region &r)
    {
        for (std::size_t i = 0; i < r.tiles.size(); ++i)
            if (r.tiles[i] != Tile::Wall)
                return static_cast<int>(i);
        return -1;
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

TEST(WorldGenTest, AllBiomesAppearAcrossChunks)
{
    std::set<Biome> biomes;
    for (int cy = 0; cy < 12; ++cy)
        for (int cx = 0; cx < 12; ++cx)
            biomes.insert(generateChunk(1, cx, cy).biome());
    EXPECT_EQ(biomes.size(), static_cast<std::size_t>(kBiomeCount));
}

TEST(WorldGenTest, RegionIsFullyConnectedAcrossSeams)
{
    // Bricks are bomb-through and interior stone only ever sits on isolated pillar
    // slots, so the non-Wall tiles of a multi-chunk region form ONE connected
    // component — no pocket is ever sealed off, on any seed.
    for (std::uint64_t seed = 1; seed <= 6; ++seed)
    {
        const Region r = materialize(seed, -1, -1, 3, 3);
        const auto seen = flood(r, firstNonWall(r), [](Tile t) { return t != Tile::Wall; });
        int reached = 0;
        for (char c : seen)
            reached += c;
        EXPECT_EQ(reached, countNonWall(r)) << "seed " << seed;
    }
}

TEST(WorldGenTest, DoorwaysConnectEveryChamberWithoutBombing)
{
    // Flood over EMPTY ONLY (i.e. no bombing) from the origin chamber's crossroads. The
    // spine + fixed doorways must carry it to the central crossroads of every chamber in
    // the region — the world is walkable chunk-to-chunk by construction.
    for (std::uint64_t seed = 1; seed <= 6; ++seed)
    {
        const int cx0 = -1, cy0 = -1, n = 3;
        const Region r = materialize(seed, cx0, cy0, n, n);
        const int centre = kChunkSize / 2;
        const auto centreIdx = [&](int cx, int cy) {
            const int lx = cx * kChunkSize + centre;
            const int ly = cy * kChunkSize + centre;
            return ly * r.width + lx;
        };
        const auto seen = flood(r, centreIdx(0, 0), [](Tile t) { return t == Tile::Empty; });
        for (int cy = 0; cy < n; ++cy)
            for (int cx = 0; cx < n; ++cx)
                EXPECT_TRUE(seen[centreIdx(cx, cy)])
                    << "seed " << seed << " chamber " << cx << "," << cy << " unreachable";
    }
}

TEST(WorldGenTest, EveryChamberIsWalledWithOpenDoorways)
{
    // The border ring is solid (stone or bomb-through brick) except the four
    // edge-midpoint doorways, which are always Empty — and at fixed cells, so a chamber's
    // doorways line up with its neighbours' across every seam.
    const std::pair<int, int> coords[] = {{0, 0}, {2, 3}, {-5, 1}, {-1, -1}};
    const int mid = kChunkSize / 2;
    const int last = kChunkSize - 1;
    for (auto [cx, cy] : coords)
    {
        const Chunk c = generateChunk(7, cx, cy);
        for (int i = 0; i < kChunkSize; ++i)
        {
            const bool topDoor = i == mid;
            EXPECT_EQ(c.at(i, 0) == Tile::Empty, topDoor) << "top " << i;
            EXPECT_EQ(c.at(i, last) == Tile::Empty, topDoor) << "bottom " << i;
            const bool sideDoor = i == mid;
            EXPECT_EQ(c.at(0, i) == Tile::Empty, sideDoor) << "left " << i;
            EXPECT_EQ(c.at(last, i) == Tile::Empty, sideDoor) << "right " << i;
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
                EXPECT_TRUE(t == Tile::Empty || t == Tile::Wall || t == Tile::Brick)
                    << "tile " << lx << "," << ly << " at chunk " << cx << "," << cy;
            }
    }
}

TEST(WorldGenTest, GoldenSeedsAreStable)
{
    // Pinned signatures for a handful of fixed seeds: the deterministic "stable
    // starting worlds" guarantee. If generation logic changes intentionally, these
    // are expected to change; an UNINTENDED change is a determinism regression.
    struct Golden { std::uint64_t seed; int cx; int cy; std::uint64_t sig; };
    const Golden golden[] = {
        {1, 0, 0, 4261681776246883336ULL},
        {2, 0, 0, 15765506637880251192ULL},
        {3, 0, 0, 14935172030040336584ULL},
        {1, 1, 0, 3752843230112716337ULL},
        {1, -1, -1, 12345632500880365950ULL},
    };
    for (const Golden &g : golden)
        EXPECT_EQ(signature(generateChunk(g.seed, g.cx, g.cy)), g.sig)
            << "seed " << g.seed << " chunk " << g.cx << "," << g.cy;
}
