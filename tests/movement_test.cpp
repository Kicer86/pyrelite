
#include "movement.h"

#include <gtest/gtest.h>

using namespace pyrelite;

TEST(MovementTest, TileOfPositiveCentres)
{
    EXPECT_EQ(tileOf(0), 0);
    EXPECT_EQ(tileOf(kSubcell), 1);
    EXPECT_EQ(tileOf(5 * kSubcell), 5);
}

TEST(MovementTest, TileOfRoundsAtTheHalfwayPoint)
{
    EXPECT_EQ(tileOf(kSubcell + kSubcell / 2 - 1), 1); // just shy of the 1/2 boundary
    EXPECT_EQ(tileOf(kSubcell + kSubcell / 2), 2);     // exact half rounds up
}

TEST(MovementTest, TileOfHandlesNegativeCoordinates)
{
    // Floor division: a position west of / north of the origin maps to the negative
    // tile it actually sits in, not toward zero (the streamed world is global).
    EXPECT_EQ(tileOf(-kSubcell), -1);
    EXPECT_EQ(tileOf(-2 * kSubcell), -2);
    EXPECT_EQ(tileOf(-kSubcell - kSubcell / 4), -1);     // off-centre within tile -1
    EXPECT_EQ(tileOf(-kSubcell - kSubcell / 2), -1);     // -1/-2 boundary rounds up
    EXPECT_EQ(tileOf(-kSubcell - kSubcell / 2 - 1), -2); // just past it
}

TEST(MovementTest, GridMoverReportsNegativeTiles)
{
    GridMover mover(-3, -5);
    EXPECT_EQ(mover.tileX(), -3);
    EXPECT_EQ(mover.tileY(), -5);
}
