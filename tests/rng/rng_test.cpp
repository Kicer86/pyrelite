
#include "rng/rng.h"

#include <gtest/gtest.h>

using namespace pyrelite;

TEST(RngTest, SameSeedSameSequence)
{
    Rng a(42);
    Rng b(42);
    for (int i = 0; i < 100; ++i)
        EXPECT_EQ(a.next(), b.next());
}

TEST(RngTest, DifferentSeedsDiffer)
{
    Rng a(1);
    Rng b(2);
    bool anyDifferent = false;
    for (int i = 0; i < 10; ++i)
        anyDifferent |= (a.next() != b.next());
    EXPECT_TRUE(anyDifferent);
}

TEST(RngTest, BelowStaysInRange)
{
    Rng rng(123);
    for (int i = 0; i < 1000; ++i)
        EXPECT_LT(rng.below(7), 7u);
}

TEST(RngTest, ChanceBounds)
{
    Rng rng(7);
    for (int i = 0; i < 100; ++i) {
        EXPECT_FALSE(rng.chance(0));
        EXPECT_TRUE(rng.chance(100));
    }
}
