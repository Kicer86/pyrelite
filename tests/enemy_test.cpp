
#include "game.h"

#include <cstdlib>

#include <gtest/gtest.h>

#include "grid.h"

using namespace pyrelite;

namespace {

// A walled 5x5 with a fully open 3x3 interior (no bricks, no pillar) so an enemy
// can roam every interior cell.
Game makeOpenRoom()
{
    Grid g(5, 5);
    for (int i = 0; i < 5; ++i) {
        g.set(i, 0, Tile::Wall);
        g.set(i, 4, Tile::Wall);
        g.set(0, i, Tile::Wall);
        g.set(4, i, Tile::Wall);
    }
    return Game(g);
}

// A 5x5 whose only empty interior cell is (1,1): the enemy there is walled in.
Game makeBoxedRoom()
{
    Grid g(5, 5);
    for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 5; ++x)
            g.set(x, y, Tile::Wall);
    g.set(1, 1, Tile::Empty);
    return Game(g);
}

} // namespace

TEST(EnemyTest, AddEnemyPlacesAtTileCentre)
{
    Game game = makeOpenRoom();
    ASSERT_TRUE(game.addEnemy(2, 3));
    ASSERT_EQ(game.enemies().size(), 1u);
    EXPECT_EQ(game.enemies().front().subX, 2 * kSubcell);
    EXPECT_EQ(game.enemies().front().subY, 3 * kSubcell);
    EXPECT_TRUE(game.hasEnemyAt(2, 3));
}

TEST(EnemyTest, AddEnemyRejectsBlockedOrOutOfBounds)
{
    Game game = makeOpenRoom();
    EXPECT_FALSE(game.addEnemy(0, 0));   // wall
    EXPECT_FALSE(game.addEnemy(-1, 1));  // out of bounds
    EXPECT_FALSE(game.addEnemy(99, 99)); // out of bounds
    EXPECT_TRUE(game.enemies().empty());
}

TEST(EnemyTest, WandersAwayFromSpawn)
{
    Game game = makeOpenRoom();
    ASSERT_TRUE(game.addEnemy(1, 1));

    bool leftStart = false;
    for (int i = 0; i < 2000 && !leftStart; ++i) {
        game.update(16);
        const Enemy &e = game.enemies().front();
        if (e.subX / kSubcell != 1 || e.subY / kSubcell != 1)
            leftStart = true;
    }
    EXPECT_TRUE(leftStart);
}

TEST(EnemyTest, BoxedEnemyStaysPut)
{
    Game game = makeBoxedRoom();
    ASSERT_TRUE(game.addEnemy(1, 1));
    for (int i = 0; i < 100; ++i)
        game.update(16);
    EXPECT_EQ(game.enemies().front().subX, kSubcell);
    EXPECT_EQ(game.enemies().front().subY, kSubcell);
}

TEST(EnemyTest, StaysGridLocked)
{
    Game game = makeOpenRoom();
    ASSERT_TRUE(game.addEnemy(1, 1));
    // Grid-locked movement only ever travels along one axis at a time, so the
    // other axis is always exactly on a tile line — never a diagonal drift.
    for (int i = 0; i < 1000; ++i) {
        game.update(16);
        const Enemy &e = game.enemies().front();
        EXPECT_TRUE(e.subX % kSubcell == 0 || e.subY % kSubcell == 0)
            << "off-grid at step " << i << ": " << e.subX << "," << e.subY;
    }
}

TEST(EnemyTest, MovementIsDeterministic)
{
    Game a = makeOpenRoom();
    Game b = makeOpenRoom();
    ASSERT_TRUE(a.addEnemy(1, 1));
    ASSERT_TRUE(b.addEnemy(1, 1));
    for (int i = 0; i < 500; ++i) {
        a.update(16);
        b.update(16);
        EXPECT_EQ(a.enemies().front().subX, b.enemies().front().subX);
        EXPECT_EQ(a.enemies().front().subY, b.enemies().front().subY);
    }
}

TEST(EnemyTest, ArenaSpawnsEnemiesAwayFromPlayerPocket)
{
    Game game(13, 11, 1);
    EXPECT_FALSE(game.enemies().empty());

    // The opening pocket the player starts in must never hold an enemy.
    EXPECT_FALSE(game.hasEnemyAt(1, 1));
    EXPECT_FALSE(game.hasEnemyAt(2, 1));
    EXPECT_FALSE(game.hasEnemyAt(1, 2));
    for (const Enemy &e : game.enemies())
    {
        const int tx = e.subX / kSubcell;
        const int ty = e.subY / kSubcell;
        EXPECT_GE(std::abs(tx - 1) + std::abs(ty - 1), 4);
        EXPECT_EQ(game.grid().at(tx, ty), Tile::Empty);
    }
}

TEST(EnemyTest, ArenaSpawnIsDeterministic)
{
    Game a(13, 11, 7);
    Game b(13, 11, 7);
    ASSERT_EQ(a.enemies().size(), b.enemies().size());
    for (std::size_t i = 0; i < a.enemies().size(); ++i)
    {
        EXPECT_EQ(a.enemies()[i].subX, b.enemies()[i].subX);
        EXPECT_EQ(a.enemies()[i].subY, b.enemies()[i].subY);
    }
}
