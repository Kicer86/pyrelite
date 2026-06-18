
#include "game.h"

#include <gtest/gtest.h>

#include "grid.h"

using namespace pyrelite;

namespace {

// A 5x5 room: solid wall border, empty interior, with a brick at (2, 1).
Game makeRoom()
{
    Grid g(5, 5);
    for (int x = 0; x < 5; ++x) {
        g.set(x, 0, Tile::Wall);
        g.set(x, 4, Tile::Wall);
    }
    for (int y = 0; y < 5; ++y) {
        g.set(0, y, Tile::Wall);
        g.set(4, y, Tile::Wall);
    }
    g.set(2, 1, Tile::Brick);
    return Game(g);
}

} // namespace

TEST(GameTest, PlayerStartsAtSpawn)
{
    Game game = makeRoom();
    EXPECT_EQ(game.playerX(), 1);
    EXPECT_EQ(game.playerY(), 1);
}

TEST(GameTest, MovesIntoEmpty)
{
    Game game = makeRoom();
    EXPECT_TRUE(game.tryMove(Direction::Down));
    EXPECT_EQ(game.playerX(), 1);
    EXPECT_EQ(game.playerY(), 2);
}

TEST(GameTest, BlockedByWall)
{
    Game game = makeRoom();
    EXPECT_FALSE(game.tryMove(Direction::Up));   // (1,0) is wall
    EXPECT_FALSE(game.tryMove(Direction::Left)); // (0,1) is wall
    EXPECT_EQ(game.playerX(), 1);
    EXPECT_EQ(game.playerY(), 1);
}

TEST(GameTest, BlockedByBrick)
{
    Game game = makeRoom();
    EXPECT_FALSE(game.tryMove(Direction::Right)); // (2,1) is brick
    EXPECT_EQ(game.playerX(), 1);
    EXPECT_EQ(game.playerY(), 1);
}

TEST(GameTest, SpawnOutOfBoundsThrows)
{
    Grid g(1, 1);
    EXPECT_THROW((Game{g}), std::invalid_argument);
}

TEST(GameTest, SpawnOnWallThrows)
{
    Grid g(5, 5);
    g.set(1, 1, Tile::Wall);
    EXPECT_THROW((Game{g}), std::invalid_argument);
}

TEST(GameTest, SpawnOnBrickThrows)
{
    Grid g(5, 5);
    g.set(1, 1, Tile::Brick);
    EXPECT_THROW((Game{g}), std::invalid_argument);
}

TEST(GameTest, GeneratedArenaSpawnIsUsable)
{
    Game game(13, 11, 1);
    EXPECT_EQ(game.playerX(), 1);
    EXPECT_EQ(game.playerY(), 1);
    // generateArena keeps (2,1) and (1,2) clear next to the spawn.
    EXPECT_TRUE(game.tryMove(Direction::Right));
    EXPECT_EQ(game.playerX(), 2);
}
