
#include "game.h"

#include <gtest/gtest.h>

#include "grid.h"

using namespace pyrelite;

namespace
{
    // An open square room: wall border, empty interior.
    Game makeOpenRoom(int size = 7)
    {
        Grid g(size, size);
        for (int i = 0; i < size; ++i)
        {
            g.set(i, 0, Tile::Wall);
            g.set(i, size - 1, Tile::Wall);
            g.set(0, i, Tile::Wall);
            g.set(size - 1, i, Tile::Wall);
        }
        return Game(g);
    }

    void moveTo(Game &game, int tx, int ty)
    {
        while (game.playerX() < tx)
            game.tryMove(Direction::Right);
        while (game.playerX() > tx)
            game.tryMove(Direction::Left);
        while (game.playerY() < ty)
            game.tryMove(Direction::Down);
        while (game.playerY() > ty)
            game.tryMove(Direction::Up);
    }
}

TEST(ExplosionTest, BlastIsACrossWithinRange)
{
    Game game = makeOpenRoom(7);
    moveTo(game, 3, 3);
    ASSERT_EQ(game.playerX(), 3);
    ASSERT_EQ(game.playerY(), 3);
    game.placeBomb(); // range 2
    game.update(2000);

    EXPECT_TRUE(game.hasExplosionAt(3, 3)); // centre
    EXPECT_TRUE(game.hasExplosionAt(3, 1)); // up 2
    EXPECT_TRUE(game.hasExplosionAt(1, 3)); // left 2
    EXPECT_TRUE(game.hasExplosionAt(5, 3)); // right 2
    EXPECT_TRUE(game.hasExplosionAt(3, 5)); // down 2
    EXPECT_FALSE(game.hasExplosionAt(3, 6)); // wall, out of range
}

TEST(ExplosionTest, BlastStopsAtWall)
{
    Game game = makeOpenRoom(7);
    moveTo(game, 1, 3); // next to the left wall (x = 0)
    game.placeBomb();
    game.update(2000);

    EXPECT_TRUE(game.hasExplosionAt(1, 3));  // centre
    EXPECT_FALSE(game.hasExplosionAt(0, 3)); // wall blocks left
    EXPECT_TRUE(game.hasExplosionAt(2, 3));  // right is clear
}

TEST(ExplosionTest, DestroysOneBrickAndStops)
{
    Grid g(7, 7);
    for (int i = 0; i < 7; ++i)
    {
        g.set(i, 0, Tile::Wall);
        g.set(i, 6, Tile::Wall);
        g.set(0, i, Tile::Wall);
        g.set(6, i, Tile::Wall);
    }
    g.set(4, 3, Tile::Brick);
    g.set(5, 3, Tile::Brick);
    Game game(g);

    moveTo(game, 3, 3);
    game.placeBomb(); // range 2, blast goes right into the bricks
    game.update(2000);

    EXPECT_TRUE(game.hasExplosionAt(4, 3));            // first brick is hit
    EXPECT_EQ(game.grid().at(4, 3), Tile::Empty);      // and destroyed
    EXPECT_FALSE(game.hasExplosionAt(5, 3));           // blast stops past it
    EXPECT_EQ(game.grid().at(5, 3), Tile::Brick);      // second brick survives
}

TEST(ExplosionTest, FlamesExpire)
{
    Game game = makeOpenRoom(7);
    moveTo(game, 3, 3);
    game.placeBomb();
    moveTo(game, 1, 1); // step clear of the blast so the run keeps ticking
    game.update(2000);
    EXPECT_TRUE(game.hasExplosionAt(3, 3));

    game.update(400); // flames live 400 ms
    EXPECT_TRUE(game.explosions().empty());
}

TEST(ExplosionTest, ChainDetonatesNeighbourBomb)
{
    Game game = makeOpenRoom(7);
    game.setBombLimit(2);

    moveTo(game, 3, 3);
    game.placeBomb();  // bomb A at (3,3), fuse 2000
    game.update(1000); // A fuse -> 1000

    moveTo(game, 5, 3);
    game.placeBomb();  // bomb B at (5,3), fuse 2000 (within A's range 2)
    game.update(1000); // A fuse -> 0 detonates; its blast reaches B and chains it

    // B detonated via the chain even though its own fuse (1000) had not elapsed.
    EXPECT_TRUE(game.hasExplosionAt(5, 5)); // B's downward arm
    EXPECT_TRUE(game.bombs().empty());
}
