
#include "arena.h"

#include <gtest/gtest.h>

using namespace pyrelite;

TEST(ArenaTest, Dimensions)
{
    Grid g = generateArena(13, 11, 1);
    EXPECT_EQ(g.width(), 13);
    EXPECT_EQ(g.height(), 11);
}

TEST(ArenaTest, SameSeedSameArena)
{
    Grid a = generateArena(13, 11, 99);
    Grid b = generateArena(13, 11, 99);
    for (int y = 0; y < 11; ++y)
        for (int x = 0; x < 13; ++x)
            EXPECT_EQ(a.at(x, y), b.at(x, y)) << "mismatch at " << x << "," << y;
}

TEST(ArenaTest, BordersAreWalls)
{
    Grid g = generateArena(13, 11, 5);
    for (int x = 0; x < 13; ++x) {
        EXPECT_EQ(g.at(x, 0), Tile::Wall);
        EXPECT_EQ(g.at(x, 10), Tile::Wall);
    }
    for (int y = 0; y < 11; ++y) {
        EXPECT_EQ(g.at(0, y), Tile::Wall);
        EXPECT_EQ(g.at(12, y), Tile::Wall);
    }
}

TEST(ArenaTest, PillarsOnEvenInteriorCells)
{
    Grid g = generateArena(13, 11, 5);
    for (int y = 2; y < 10; y += 2)
        for (int x = 2; x < 12; x += 2)
            EXPECT_EQ(g.at(x, y), Tile::Wall) << "pillar at " << x << "," << y;
}

TEST(ArenaTest, SpawnCornerIsClear)
{
    Grid g = generateArena(13, 11, 5);
    EXPECT_EQ(g.at(1, 1), Tile::Empty);
    EXPECT_EQ(g.at(2, 1), Tile::Empty);
    EXPECT_EQ(g.at(1, 2), Tile::Empty);
}

TEST(ArenaTest, TooSmallThrows)
{
    EXPECT_THROW(generateArena(4, 11, 1), std::invalid_argument);
    EXPECT_THROW(generateArena(13, 4, 1), std::invalid_argument);
    EXPECT_THROW(generateArena(3, 3, 1), std::invalid_argument);
}

TEST(ArenaTest, MinimumSizeWorks)
{
    Grid g = generateArena(5, 5, 1);
    EXPECT_EQ(g.width(), 5);
    EXPECT_EQ(g.height(), 5);
    EXPECT_EQ(g.at(1, 1), Tile::Empty);
    EXPECT_EQ(g.at(2, 1), Tile::Empty);
    EXPECT_EQ(g.at(1, 2), Tile::Empty);
}

TEST(ArenaTest, OnlyKnownTiles)
{
    Grid g = generateArena(13, 11, 5);
    for (int y = 0; y < 11; ++y)
        for (int x = 0; x < 13; ++x) {
            const Tile t = g.at(x, y);
            EXPECT_TRUE(t == Tile::Empty || t == Tile::Wall || t == Tile::Brick);
        }
}
