
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
        std::uint64_t h = 1469598103934665603ULL; // FNV-1a offset basis
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

    // Tiles reachable from the first non-Wall cell, treating Empty AND Brick as
    // passable (a brick is bomb-through, so it never partitions the world).
    int floodNonWall(const Region &r)
    {
        std::vector<char> seen(r.tiles.size(), 0);
        int start = -1;
        for (std::size_t i = 0; i < r.tiles.size(); ++i)
            if (r.tiles[i] != Tile::Wall)
            {
                start = static_cast<int>(i);
                break;
            }
        if (start < 0)
            return 0;

        std::vector<int> stack{start};
        int count = 0;
        while (!stack.empty())
        {
            const int idx = stack.back();
            stack.pop_back();
            if (seen[idx] || r.tiles[idx] == Tile::Wall)
                continue;
            seen[idx] = 1;
            ++count;

            const int lx = idx % r.width;
            const int ly = idx / r.width;
            if (lx > 0)             stack.push_back(idx - 1);
            if (lx < r.width - 1)   stack.push_back(idx + 1);
            if (ly > 0)             stack.push_back(idx - r.width);
            if (ly < r.height - 1)  stack.push_back(idx + r.width);
        }
        return count;
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
    for (int cy = 0; cy < 8; ++cy)
        for (int cx = 0; cx < 8; ++cx)
            biomes.insert(generateChunk(1, cx, cy).biome());
    EXPECT_EQ(biomes.size(), static_cast<std::size_t>(kBiomeCount));
}

TEST(WorldGenTest, RegionIsFullyConnectedAcrossSeams)
{
    // No biome adds an off-lattice wall, so the non-Wall tiles of a multi-chunk
    // region form ONE connected component — seams join and no pocket is sealed.
    for (std::uint64_t seed = 1; seed <= 6; ++seed)
    {
        const Region r = materialize(seed, -1, -1, 3, 3);
        EXPECT_EQ(floodNonWall(r), countNonWall(r)) << "seed " << seed;
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

TEST(WorldGenTest, DenseBiomeLaysLatticeAtGlobalEvenCells)
{
    // Find a Thicket chunk (keeps every pillar) and confirm the indestructible
    // lattice sits exactly on global even/even cells — i.e. it is continuous across
    // chunk seams regardless of which chunk you are in.
    bool checked = false;
    for (int cx = 0; cx < 16 && !checked; ++cx)
    {
        const Chunk chunk = generateChunk(1, cx, 0);
        if (chunk.biome() != Biome::Thicket)
            continue;
        checked = true;
        for (int ly = 0; ly < kChunkSize; ++ly)
            for (int lx = 0; lx < kChunkSize; ++lx)
            {
                const int gx = cx * kChunkSize + lx;
                const int gy = ly;
                if (gx % 2 == 0 && gy % 2 == 0)
                    EXPECT_EQ(chunk.at(lx, ly), Tile::Wall) << "pillar " << gx << "," << gy;
            }
    }
    EXPECT_TRUE(checked) << "no Thicket chunk found to probe the lattice";
}

TEST(WorldGenTest, GoldenSeedsAreStable)
{
    // Pinned signatures for a handful of fixed seeds: the deterministic "stable
    // starting worlds" guarantee. If generation logic changes intentionally, these
    // are expected to change; an UNINTENDED change is a determinism regression.
    struct Golden { std::uint64_t seed; int cx; int cy; std::uint64_t sig; };
    const Golden golden[] = {
        {1, 0, 0, 1524707709870747195ULL},
        {2, 0, 0, 18128534612142643051ULL},
        {3, 0, 0, 12153168450474582099ULL},
        {1, 1, 0, 14364027955256610545ULL},
        {1, -1, -1, 3376190721455917189ULL},
    };
    for (const Golden &g : golden)
        EXPECT_EQ(signature(generateChunk(g.seed, g.cx, g.cy)), g.sig)
            << "seed " << g.seed << " chunk " << g.cx << "," << g.cy;
}
