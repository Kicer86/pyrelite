
// Run score (M4 meta-economy source): deterministic, integer. Kills pay more the
// deeper the zone they fell in, and pushing outward scores the furthest Chebyshev
// distance reached from the origin (the peak, never the current position) plus a
// super-linear bonus in the deepest tier touched. Bounded arenas stay at tier 0,
// which isolates the kill and linear-reach terms on hand-carved grids; the streamed
// world exercises the tier-scaled deep bonus (see streamed_game_test).

#include "game/game.h"

#include <gtest/gtest.h>

#include "grid/grid.h"

using namespace pyrelite;

namespace {

// An all-wall grid; tests carve out only the cells they need.
Grid solid(int width, int height)
{
    Grid g(width, height);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            g.set(x, y, Tile::Wall);
    return g;
}

// A straight east corridor along row 1: (1,1)..(width-2,1) open, the rest wall, so
// the player can stride out from the origin and back along a known distance.
Game makeCorridor(int width)
{
    Grid g = solid(width, 3);
    for (int x = 1; x < width - 1; ++x)
        g.set(x, 1, Tile::Empty);
    return Game(g);
}

// A room where bombing the enemy at (3,1) from (2,1) and ducking to (1,2) kills it,
// while a boxed survivor at (5,1) keeps the run live. With withTarget=false the (3,1)
// enemy is omitted, so the identical walk and blast kill nothing — the twin that
// isolates the kill term from the depth term.
//   # # # # # # #
//   # P . E # S #
//   # . # # # # #
Game makeKillRoom(bool withTarget)
{
    Grid g = solid(7, 4);
    g.set(1, 1, Tile::Empty);
    g.set(2, 1, Tile::Empty);
    g.set(3, 1, Tile::Empty);
    g.set(1, 2, Tile::Empty);
    g.set(5, 1, Tile::Empty); // boxed survivor pocket: every neighbour is a wall
    Game game(g);
    if (withTarget)
        EXPECT_TRUE(game.addEnemy(3, 1));
    EXPECT_TRUE(game.addEnemy(5, 1));
    return game;
}

// Light the bomb at (2,1), retreat to (1,2) clear of the cross, and detonate. Kills
// the enemy on (3,1) if one is there; otherwise the blast just clears air.
void bombRow(Game &game)
{
    ASSERT_TRUE(game.tryMove(Direction::Right)); // to (2,1)
    ASSERT_TRUE(game.placeBomb());
    ASSERT_TRUE(game.tryMove(Direction::Left));  // back to (1,1)
    ASSERT_TRUE(game.tryMove(Direction::Down));  // to (1,2), clear
    game.update(2000);                           // detonate
}

} // namespace

TEST(ScoreTest, FreshRunScoresZero)
{
    Game game = makeCorridor(8);
    EXPECT_EQ(game.score(), 0);
    EXPECT_EQ(game.maxDepth(), 0);
}

TEST(ScoreTest, MaxDepthTracksFurthestReachedNotCurrent)
{
    Game game = makeCorridor(8);
    ASSERT_TRUE(game.tryMove(Direction::Right)); // (2,1), depth 1
    ASSERT_TRUE(game.tryMove(Direction::Right)); // (3,1), depth 2
    ASSERT_TRUE(game.tryMove(Direction::Right)); // (4,1), depth 3
    EXPECT_EQ(game.maxDepth(), 3);

    ASSERT_TRUE(game.tryMove(Direction::Left));  // back to (3,1)
    ASSERT_TRUE(game.tryMove(Direction::Left));  // back to (2,1)
    EXPECT_EQ(game.maxDepth(), 3);               // the peak, not the current depth 1
}

TEST(ScoreTest, PushingFurtherScoresStrictlyHigher)
{
    Game shallow = makeCorridor(10);
    ASSERT_TRUE(shallow.tryMove(Direction::Right)); // depth 1
    const int shallowScore = shallow.score();

    Game deep = makeCorridor(10);
    for (int i = 0; i < 5; ++i)
        ASSERT_TRUE(deep.tryMove(Direction::Right)); // depth 5
    EXPECT_GT(deep.score(), shallowScore);           // reach alone raises the score
}

TEST(ScoreTest, AKillScoresAboveTheSameWalkWithoutOne)
{
    // Identical geometry, walk and blast — only one room has an enemy to kill — so the
    // gap between the two scores is exactly the kill's worth, isolated from depth.
    Game withKill = makeKillRoom(true);
    bombRow(withKill);
    ASSERT_EQ(withKill.enemies().size(), 1u); // the boxed survivor; target died

    Game noKill = makeKillRoom(false);
    bombRow(noKill);
    ASSERT_EQ(noKill.enemies().size(), 1u);   // nothing was there to kill

    EXPECT_EQ(withKill.maxDepth(), noKill.maxDepth()); // same path, same reach
    EXPECT_GT(withKill.score(), noKill.score());       // the kill is what pays
}
