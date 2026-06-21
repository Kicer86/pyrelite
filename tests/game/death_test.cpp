
#include "game/game.h"

#include <optional>

#include <gtest/gtest.h>

#include "grid/grid.h"

using namespace pyrelite;

namespace {

// Build a grid of the given size that is all walls, so tests can carve out only
// the empty cells they need.
Grid solid(int width, int height)
{
    Grid g(width, height);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            g.set(x, y, Tile::Wall);
    return g;
}

// A walled room with an open interior, for free movement / self-blast tests.
Game makeOpenRoom(int size = 7)
{
    Grid g(size, size);
    for (int i = 0; i < size; ++i) {
        g.set(i, 0, Tile::Wall);
        g.set(i, size - 1, Tile::Wall);
        g.set(0, i, Tile::Wall);
        g.set(size - 1, i, Tile::Wall);
    }
    return Game(g);
}

} // namespace

TEST(DeathTest, EnemyDiesInBlastAndClearsTheArena)
{
    // Layout (E = enemy, . = empty): the enemy at (3,1) can only reach (2,1), so a
    // bomb dropped there walls it in and its rightward arm catches the enemy.
    //   # # # # # #
    //   # . . E # #
    //   # . # # # #
    Grid g = solid(6, 4);
    g.set(1, 1, Tile::Empty);
    g.set(2, 1, Tile::Empty);
    g.set(3, 1, Tile::Empty);
    g.set(1, 2, Tile::Empty);
    Game game(g);
    ASSERT_TRUE(game.addEnemy(3, 1));

    game.tryMove(Direction::Right); // to (2,1)
    ASSERT_TRUE(game.placeBomb());  // bomb on (2,1) boxes the enemy in
    game.tryMove(Direction::Left);  // back to (1,1)
    game.tryMove(Direction::Down);  // to (1,2), clear of the blast

    game.update(2000); // detonate

    EXPECT_TRUE(game.enemies().empty());
    EXPECT_EQ(game.state(), GameState::Won);
    EXPECT_EQ(game.playerX(), 1);
    EXPECT_EQ(game.playerY(), 2);
}

TEST(DeathTest, PlayerDiesOnEnemyContact)
{
    // A 1-wide corridor: the enemy is funnelled left into the stationary player.
    //   # # # # #
    //   # P . E #
    //   # # # # #
    Grid g = solid(5, 3);
    g.set(1, 1, Tile::Empty);
    g.set(2, 1, Tile::Empty);
    g.set(3, 1, Tile::Empty);
    Game game(g);
    ASSERT_TRUE(game.addEnemy(3, 1));

    for (int i = 0; i < 1000 && game.state() == GameState::Playing; ++i)
        game.update(16);

    EXPECT_EQ(game.state(), GameState::Lost);
}

TEST(DeathTest, PlayerDiesInOwnBlast)
{
    Game game = makeOpenRoom(5);
    ASSERT_TRUE(game.placeBomb()); // standing on the bomb at the spawn
    game.update(2000);             // its own flame reaches the player
    EXPECT_EQ(game.state(), GameState::Lost);
}

TEST(DeathTest, EndedRunIsFrozen)
{
    Game game = makeOpenRoom(5);
    game.placeBomb();
    game.update(2000);
    ASSERT_EQ(game.state(), GameState::Lost);

    const int subX = game.playerSubX();
    const int subY = game.playerSubY();
    game.setMoveDirection(Direction::Right);
    EXPECT_FALSE(game.update(16)); // no ticking after the run ends
    EXPECT_EQ(game.playerSubX(), subX);
    EXPECT_EQ(game.playerSubY(), subY);
}

TEST(DeathTest, NoEnemiesNeverAutoWins)
{
    // A run that never had enemies must not "win" by having an empty enemy list.
    Game game = makeOpenRoom(7);
    game.setMoveDirection(Direction::Right);
    for (int i = 0; i < 200; ++i)
        game.update(16);
    EXPECT_EQ(game.state(), GameState::Playing);
}
