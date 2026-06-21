
#include "world.h"

#include <cstdint>
#include <optional>
#include <utility>

#include <gtest/gtest.h>

#include "chunk.h"
#include "delta_store.h"
#include "grid.h"
#include "world_gen.h"

using namespace pyrelite;

namespace
{
    // First brick tile found scanning a small global area — a target to "bomb".
    std::optional<std::pair<int, int>> findBrick(World &world, int span)
    {
        for (int gy = 0; gy < span; ++gy)
            for (int gx = 0; gx < span; ++gx)
                if (world.at(gx, gy) == Tile::Brick)
                    return std::make_pair(gx, gy);
        return std::nullopt;
    }
}

TEST(DeltaStoreTest, RecordedValueIsReturned)
{
    DeltaStore store;
    store.record(5, 7, Tile::Empty);
    EXPECT_EQ(store.at(5, 7), std::optional<Tile>(Tile::Empty));
    EXPECT_EQ(store.size(), 1u);
}

TEST(DeltaStoreTest, MissReturnsNullopt)
{
    DeltaStore store;
    EXPECT_FALSE(store.at(0, 0).has_value());
}

TEST(DeltaStoreTest, RecordOverwritesInPlace)
{
    DeltaStore store;
    store.record(1, 1, Tile::Brick);
    store.record(1, 1, Tile::Empty);
    EXPECT_EQ(store.at(1, 1), std::optional<Tile>(Tile::Empty));
    EXPECT_EQ(store.size(), 1u);
}

TEST(WorldTest, ChunkCoordFloorsNegatives)
{
    EXPECT_EQ(World::chunkCoord(0), 0);
    EXPECT_EQ(World::chunkCoord(kChunkSize - 1), 0);
    EXPECT_EQ(World::chunkCoord(kChunkSize), 1);
    EXPECT_EQ(World::chunkCoord(-1), -1);
    EXPECT_EQ(World::chunkCoord(-kChunkSize), -1);
    EXPECT_EQ(World::chunkCoord(-kChunkSize - 1), -2);
}

TEST(WorldTest, AtMatchesGeneratorForUntouchedTiles)
{
    World world(123);
    const std::pair<int, int> coords[] = {
        {0, 0}, {3, 5}, {kChunkSize, 0}, {-1, -1}, {-kChunkSize - 3, 2}, {7, -kChunkSize}};
    for (auto [gx, gy] : coords)
    {
        const int cx = World::chunkCoord(gx);
        const int cy = World::chunkCoord(gy);
        const Chunk base = generateChunk(123, cx, cy);
        EXPECT_EQ(world.at(gx, gy), base.at(gx - cx * kChunkSize, gy - cy * kChunkSize))
            << "global " << gx << "," << gy;
    }
}

TEST(WorldTest, SetRecordsADeltaAndShowsTheChange)
{
    World world(1);
    world.set(40, 40, Tile::Empty);
    EXPECT_EQ(world.at(40, 40), Tile::Empty);
    EXPECT_EQ(world.deltaCount(), 1u);
}

TEST(WorldTest, ChangesSurviveUnloadReload)
{
    // The anti-farm guarantee: a tile the player cleared stays cleared after its
    // chunk is unloaded and regenerated, while untouched tiles regenerate as before.
    World world(1);
    const auto brick = findBrick(world, 2 * kChunkSize);
    ASSERT_TRUE(brick.has_value());
    const auto [bx, by] = *brick;

    // An untouched tile in the same chunk, to confirm the base still regenerates.
    const int wx = bx ^ 1; // a different cell in the same chunk
    const Tile untouchedBefore = world.at(wx, by);

    world.set(bx, by, Tile::Empty); // "bomb" the brick

    // Stream far away so the origin chunk is evicted, then come back.
    world.ensureWindow(1000, 1000, 1);
    EXPECT_EQ(world.residentChunkCount(), 9u); // only the faraway 3x3 window

    EXPECT_EQ(world.at(bx, by), Tile::Empty) << "cleared tile must not regrow";
    EXPECT_EQ(world.at(wx, by), untouchedBefore) << "untouched tile regenerates as before";
    EXPECT_EQ(world.deltaCount(), 1u);
}

TEST(WorldTest, EnsureWindowLoadsAndUnloadsToBoundResidency)
{
    World world(1);
    world.ensureWindow(0, 0, 2);
    EXPECT_EQ(world.residentChunkCount(), 25u); // (2*2+1)^2

    world.ensureWindow(50, 50, 2);
    EXPECT_EQ(world.residentChunkCount(), 25u); // old chunks dropped, new loaded
}

TEST(WorldTest, DeltaCountTracksDistinctChanges)
{
    World world(1);
    world.set(10, 10, Tile::Empty);
    world.set(11, 10, Tile::Empty);
    EXPECT_EQ(world.deltaCount(), 2u);
    world.set(10, 10, Tile::Brick); // same tile again — not a new delta
    EXPECT_EQ(world.deltaCount(), 2u);
}
