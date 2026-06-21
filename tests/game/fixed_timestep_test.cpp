
#include "game/fixed_timestep.h"

#include <gtest/gtest.h>

using namespace pyrelite;

TEST(FixedTimestepTest, NonPositiveFrameYieldsNoSteps)
{
    FixedTimestep step(16, 250);
    EXPECT_EQ(step.advance(0.0), 0);
    EXPECT_EQ(step.advance(-100.0), 0);
}

TEST(FixedTimestepTest, NominalFrameRunsOneStep)
{
    FixedTimestep step(16, 250);
    EXPECT_EQ(step.advance(16.0), 1);
}

TEST(FixedTimestepTest, SubStepFramesAccumulate)
{
    FixedTimestep step(16, 250);
    EXPECT_EQ(step.advance(10.0), 0); // accumulator 10
    EXPECT_EQ(step.advance(10.0), 1); // accumulator 20 -> 1 step, 4 left
    EXPECT_EQ(step.advance(11.0), 0); // accumulator 15
    EXPECT_EQ(step.advance(1.0), 1);  // accumulator 16 -> 1 step
}

TEST(FixedTimestepTest, LargeFrameRunsMultipleSteps)
{
    FixedTimestep step(16, 250);
    EXPECT_EQ(step.advance(50.0), 3); // 48 consumed, 2 left
}

// Fractional frames must not drift: the total number of steps over a span
// equals floor(totalMs / stepMs), with the remainder carried across calls.
TEST(FixedTimestepTest, FractionalFramesDoNotDrift)
{
    FixedTimestep step(16, 1000);
    int total = 0;
    for (int i = 0; i < 100; ++i)
        total += step.advance(16.5); // 1650 ms total
    EXPECT_EQ(total, 103);           // floor(1650 / 16)
}

// A runaway frame (e.g. a backgrounded web tab resuming) is clamped to
// maxFrameMs, so it never spins out thousands of catch-up steps.
TEST(FixedTimestepTest, RunawayFrameIsClamped)
{
    FixedTimestep step(16, 250);
    EXPECT_EQ(step.advance(100000.0), 15); // floor(250 / 16), not ~6250
}

TEST(FixedTimestepTest, StepMsIsFlooredAtOne)
{
    FixedTimestep step(0, 250);
    EXPECT_GE(step.stepMs(), 1);
    EXPECT_GT(step.advance(100.0), 0); // terminates, no infinite loop
}
