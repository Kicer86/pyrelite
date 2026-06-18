
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

TEST(GameTest, QueuedMoveAppliesOnUpdate)
{
    Game game = makeRoom();
    game.queueMove(Direction::Down);
    EXPECT_EQ(game.playerY(), 1); // not applied until the tick
    EXPECT_TRUE(game.update(16));
    EXPECT_EQ(game.playerY(), 2);
}

TEST(GameTest, QueuedBombPlacedOnUpdate)
{
    Game game = makeRoom();
    game.queueBomb();
    EXPECT_TRUE(game.bombs().empty()); // not applied until the tick
    EXPECT_TRUE(game.update(16));
    EXPECT_EQ(game.bombs().size(), 1u);
}

TEST(GameTest, QueuedBombThenMoveDropsAndRuns)
{
    Game game = makeRoom();
    game.queueBomb();
    game.queueMove(Direction::Down);
    EXPECT_TRUE(game.update(16));
    ASSERT_EQ(game.bombs().size(), 1u);
    EXPECT_EQ(game.bombs().front().x, 1);
    EXPECT_EQ(game.bombs().front().y, 1); // bomb left on the spawn cell
    EXPECT_EQ(game.playerX(), 1);
    EXPECT_EQ(game.playerY(), 2);         // player stepped off it
}

TEST(GameTest, QueuedMoveIsConsumedOnce)
{
    Game game = makeRoom();
    game.queueMove(Direction::Down);
    EXPECT_TRUE(game.update(16));
    EXPECT_EQ(game.playerY(), 2);
    game.update(16); // nothing queued -> player stays put
    EXPECT_EQ(game.playerY(), 2);
}

TEST(GameTest, LatestQueuedMoveWins)
{
    Game game = makeRoom();
    game.queueMove(Direction::Down);  // valid, but...
    game.queueMove(Direction::Right); // ...overwrites it; (2,1) is a brick
    EXPECT_FALSE(game.update(16));     // Right is blocked, nothing changed
    EXPECT_EQ(game.playerX(), 1);
    EXPECT_EQ(game.playerY(), 1);      // Down was discarded
}
