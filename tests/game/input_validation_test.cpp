
#include "game/game.h"

#include <gtest/gtest.h>

#include "grid/grid.h"

using namespace pyrelite;

namespace
{
    Game makeRoom()
    {
        Grid g(5, 5);
        for (int x = 0; x < 5; ++x)
        {
            g.set(x, 0, Tile::Wall);
            g.set(x, 4, Tile::Wall);
        }
        for (int y = 0; y < 5; ++y)
        {
            g.set(0, y, Tile::Wall);
            g.set(4, y, Tile::Wall);
        }
        return Game(g);
    }
}

TEST(InputValidationTest, NegativeDeltaMsIsIgnored)
{
    Game game = makeRoom();
    ASSERT_TRUE(game.placeBomb());
    EXPECT_FALSE(game.update(-100));
    EXPECT_EQ(game.bombs().size(), 1u);
}

TEST(InputValidationTest, ZeroDeltaMsIsIgnored)
{
    Game game = makeRoom();
    ASSERT_TRUE(game.placeBomb());
    EXPECT_FALSE(game.update(0));
    EXPECT_EQ(game.bombs().size(), 1u);
}

TEST(InputValidationTest, SetBombLimitClampsToOne)
{
    Game game = makeRoom();
    game.setBombLimit(0);
    EXPECT_EQ(game.bombLimit(), 1);
    game.setBombLimit(-5);
    EXPECT_EQ(game.bombLimit(), 1);
}

TEST(InputValidationTest, SetBombRangeClampsToOne)
{
    Game game = makeRoom();
    game.setBombRange(0);
    EXPECT_EQ(game.bombRange(), 1);
    game.setBombRange(-3);
    EXPECT_EQ(game.bombRange(), 1);
}
