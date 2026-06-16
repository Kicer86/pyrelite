
#include "grid.h"

#include <gtest/gtest.h>

#include <stdexcept>

using namespace pyrelite;

TEST(GridTest, Dimensions)
{
    Grid grid(5, 3);
    EXPECT_EQ(grid.width(), 5);
    EXPECT_EQ(grid.height(), 3);
}

TEST(GridTest, Bounds)
{
    Grid grid(5, 3);
    EXPECT_TRUE(grid.inBounds(0, 0));
    EXPECT_TRUE(grid.inBounds(4, 2));
    EXPECT_FALSE(grid.inBounds(5, 0));
    EXPECT_FALSE(grid.inBounds(0, 3));
    EXPECT_FALSE(grid.inBounds(-1, 0));
}

TEST(GridTest, CellsStartEmpty)
{
    Grid grid(5, 3);
    EXPECT_EQ(grid.at(0, 0), Tile::Empty);
    EXPECT_EQ(grid.at(4, 2), Tile::Empty);
}

TEST(GridTest, SetAndReadBack)
{
    Grid grid(5, 3);
    grid.set(2, 1, Tile::Brick);
    EXPECT_EQ(grid.at(2, 1), Tile::Brick);
    EXPECT_EQ(grid.at(2, 0), Tile::Empty);
}

TEST(GridTest, OutOfRangeAccessThrows)
{
    Grid grid(5, 3);
    EXPECT_THROW(grid.at(5, 0), std::out_of_range);
    EXPECT_THROW(grid.set(0, 3, Tile::Wall), std::out_of_range);
}

TEST(GridTest, NonPositiveDimensionsThrow)
{
    EXPECT_THROW(Grid(0, 5), std::invalid_argument);
    EXPECT_THROW(Grid(5, -1), std::invalid_argument);
}
