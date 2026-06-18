
#include "game.h"

#include <optional>

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

TEST(GameTest, HeldDirectionMovesContinuously)
{
    Game game = makeRoom();
    game.setMoveDirection(Direction::Down);
    EXPECT_TRUE(game.update(16));
    // Continuous: a fraction of a tile in one tick, not a whole cell — and
    // grid-locked, so the perpendicular axis never drifts. (Speed-agnostic so a
    // balance tweak to the base move speed doesn't break the test.)
    EXPECT_GT(game.playerSubY(), kSubcell);
    EXPECT_LT(game.playerSubY(), 2 * kSubcell);
    EXPECT_EQ(game.playerSubX(), kSubcell);
}

TEST(GameTest, HeldDirectionBlockedByWallDoesNotMove)
{
    Game game = makeRoom();
    game.setMoveDirection(Direction::Up); // (1,0) is a wall
    for (int i = 0; i < 10; ++i)
        EXPECT_FALSE(game.update(16));
    EXPECT_EQ(game.playerSubY(), kSubcell); // never left the spawn centre
}

TEST(GameTest, FinishesStepAfterRelease)
{
    Game game = makeRoom();
    game.setMoveDirection(Direction::Down);
    EXPECT_TRUE(game.update(16));        // committed, now off-centre
    game.setMoveDirection(std::nullopt); // release mid-tile
    for (int i = 0; i < 40; ++i)
        game.update(16);
    EXPECT_EQ(game.playerSubY(), 2 * kSubcell); // finished the committed step...
    EXPECT_EQ(game.playerY(), 2);
    EXPECT_FALSE(game.update(16));              // ...and then stays put
    EXPECT_EQ(game.playerSubY(), 2 * kSubcell);
}

TEST(GameTest, TurnsOnlyWhenCentred)
{
    Game game = makeRoom();
    game.setMoveDirection(Direction::Down);
    game.update(16);                        // off-centre, heading down
    game.setMoveDirection(Direction::Right);
    game.update(16);                        // still finishing the down step
    EXPECT_EQ(game.playerSubX(), kSubcell); // no diagonal: did not turn mid-tile
    for (int i = 0; i < 60; ++i)
        game.update(16);
    EXPECT_GE(game.playerX(), 2);           // turned right once centred at (1,2)
    EXPECT_EQ(game.playerY(), 2);
}

TEST(GameTest, QueuedBombPlacedOnUpdate)
{
    Game game = makeRoom();
    game.queueBomb();
    EXPECT_TRUE(game.bombs().empty()); // not applied until the tick
    EXPECT_TRUE(game.update(16));
    ASSERT_EQ(game.bombs().size(), 1u);
    EXPECT_EQ(game.bombs().front().x, 1);
    EXPECT_EQ(game.bombs().front().y, 1);
}

TEST(GameTest, QueuedBombThenHeldMoveDropsAndRuns)
{
    Game game = makeRoom();
    game.queueBomb();
    game.setMoveDirection(Direction::Down);
    EXPECT_TRUE(game.update(16));
    ASSERT_EQ(game.bombs().size(), 1u);
    EXPECT_EQ(game.bombs().front().x, 1);
    EXPECT_EQ(game.bombs().front().y, 1);   // bomb stays on the spawn tile
    EXPECT_GT(game.playerSubY(), kSubcell);  // player has begun leaving it
    game.setMoveDirection(std::nullopt);
    for (int i = 0; i < 40; ++i)
        game.update(16);
    EXPECT_EQ(game.playerY(), 2);            // finished the step off the bomb
    EXPECT_EQ(game.playerSubY(), 2 * kSubcell);
}
