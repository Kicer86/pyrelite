
#include "game.h"

#include <gtest/gtest.h>

#include "grid.h"

using namespace pyrelite;

namespace
{
    // An open room (wall border, empty interior) so the player can move freely.
    Game makeOpenRoom()
    {
        Grid g(7, 7);
        for (int i = 0; i < 7; ++i)
        {
            g.set(i, 0, Tile::Wall);
            g.set(i, 6, Tile::Wall);
            g.set(0, i, Tile::Wall);
            g.set(6, i, Tile::Wall);
        }
        return Game(g);
    }

    int bombCount(const Game &game)
    {
        return static_cast<int>(game.bombs().size());
    }
}

TEST(BombTest, PlaceBombAtPlayer)
{
    Game game = makeOpenRoom();
    EXPECT_TRUE(game.placeBomb());
    EXPECT_EQ(bombCount(game), 1);
    EXPECT_TRUE(game.hasBombAt(1, 1));
}

TEST(BombTest, RespectsBombLimit)
{
    Game game = makeOpenRoom();
    EXPECT_TRUE(game.placeBomb());
    game.tryMove(Direction::Right);
    EXPECT_FALSE(game.placeBomb()); // still at the limit of 1
    EXPECT_EQ(bombCount(game), 1);
}

TEST(BombTest, BombBlocksWalkingBack)
{
    Game game = makeOpenRoom();
    game.placeBomb();                            // bomb on (1,1)
    EXPECT_TRUE(game.tryMove(Direction::Right)); // step off to (2,1)
    EXPECT_FALSE(game.tryMove(Direction::Left)); // can't step back onto the bomb
}

TEST(BombTest, FuseRemovesBomb)
{
    Game game = makeOpenRoom();
    game.placeBomb();
    EXPECT_FALSE(game.update(1999)); // still ticking
    EXPECT_EQ(bombCount(game), 1);
    EXPECT_TRUE(game.update(1)); // fuse hits 0 -> removed
    EXPECT_EQ(bombCount(game), 0);
}

TEST(BombTest, SlotFreedAfterDetonation)
{
    Game game = makeOpenRoom();
    game.placeBomb();
    game.update(2000); // first bomb gone
    EXPECT_TRUE(game.placeBomb());
}
