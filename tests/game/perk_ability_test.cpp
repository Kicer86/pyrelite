
// Perk abilities (M3): perks are qualitative, build-defining upgrades, not numeric
// stat bumps like brick power-ups. Each is exercised here directly through its public
// state seam (setPierceBlast, ...) on a hand-carved grid, so the mechanic is tested in
// isolation from the level-up/claim flow (covered by progression_test).

#include "game/game.h"

#include <gtest/gtest.h>

#include "grid/grid.h"

using namespace pyrelite;

namespace
{
    // A wall-bordered corridor with three bricks in a line east of the spawn:
    //   # # # # # # #
    //   # P B B B . #   player (1,1); bricks (2,1)(3,1)(4,1); (5,1) open
    //   # # # # # # #
    // The player sits on the bomb at (1,1); the blast travels east along the bricks.
    Game makeBrickLine()
    {
        Grid g(7, 3);
        for (int x = 0; x < 7; ++x)
        {
            g.set(x, 0, Tile::Wall);
            g.set(x, 2, Tile::Wall);
        }
        g.set(0, 1, Tile::Wall);
        g.set(6, 1, Tile::Wall);
        for (int x = 1; x <= 5; ++x)
            g.set(x, 1, Tile::Empty);
        g.set(2, 1, Tile::Brick);
        g.set(3, 1, Tile::Brick);
        g.set(4, 1, Tile::Brick);

        Game game(g);
        game.setBombRange(3);          // long enough to span all three bricks
        game.setPowerUpDropPercent(0); // no stray drops to reason about
        return game;
    }
}

TEST(PerkAbilityTest, BlastStopsAtTheFirstBrickByDefault)
{
    Game game = makeBrickLine();
    ASSERT_TRUE(game.placeBomb());
    game.update(2000); // detonate

    EXPECT_TRUE(game.hasExplosionAt(2, 1));   // first brick takes the hit...
    EXPECT_FALSE(game.hasExplosionAt(3, 1));  // ...and the arm spends itself there
    EXPECT_EQ(game.tileAt(2, 1), Tile::Empty);
    EXPECT_EQ(game.tileAt(3, 1), Tile::Brick); // bricks behind it still stand
    EXPECT_EQ(game.tileAt(4, 1), Tile::Brick);
}

TEST(PerkAbilityTest, PierceBlastTearsThroughBricksToFullRange)
{
    Game game = makeBrickLine();
    game.setPierceBlast(true);
    ASSERT_TRUE(game.placeBomb());
    game.update(2000); // detonate

    EXPECT_TRUE(game.hasExplosionAt(2, 1));
    EXPECT_TRUE(game.hasExplosionAt(3, 1));
    EXPECT_TRUE(game.hasExplosionAt(4, 1)); // reached the full range through the bricks
    EXPECT_EQ(game.tileAt(2, 1), Tile::Empty);
    EXPECT_EQ(game.tileAt(3, 1), Tile::Empty);
    EXPECT_EQ(game.tileAt(4, 1), Tile::Empty); // and levelled every brick on the way
}

TEST(PerkAbilityTest, PierceBlastStillStopsAtSolidWalls)
{
    // A solid wall, unlike a brick, always halts the arm even with Pierce.
    Grid g(7, 3);
    for (int x = 0; x < 7; ++x)
    {
        g.set(x, 0, Tile::Wall);
        g.set(x, 2, Tile::Wall);
    }
    g.set(0, 1, Tile::Wall);
    for (int x = 1; x <= 6; ++x)
        g.set(x, 1, Tile::Empty);
    g.set(3, 1, Tile::Wall); // a solid wall mid-corridor

    Game game(g);
    game.setBombRange(4);
    game.setPierceBlast(true);
    ASSERT_TRUE(game.placeBomb());
    game.update(2000);

    EXPECT_TRUE(game.hasExplosionAt(2, 1));
    EXPECT_FALSE(game.hasExplosionAt(3, 1)); // the wall itself is never set alight
    EXPECT_FALSE(game.hasExplosionAt(4, 1)); // nor anything beyond it
}
