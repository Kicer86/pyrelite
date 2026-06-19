
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

// The player is boxed alone in the (1,1) pocket (its neighbours stay walls, so an
// enemy can never reach it) while the enemy gets a separate open block to roam.
// Keeps enemy-motion tests free of incidental player-contact deaths.
Game makeRoamRoom()
{
    Grid g(7, 7);
    for (int y = 0; y < 7; ++y)
        for (int x = 0; x < 7; ++x)
            g.set(x, y, Tile::Wall);
    g.set(1, 1, Tile::Empty); // player pocket, walled off
    for (int y = 3; y <= 5; ++y)
        for (int x = 3; x <= 5; ++x)
            g.set(x, y, Tile::Empty); // roaming block
    return Game(g);
}

// The player and a single isolated enemy cell, each walled in: nothing can move.
Game makeBoxedRoom()
{
    Grid g(5, 5);
    for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 5; ++x)
            g.set(x, y, Tile::Wall);
    g.set(1, 1, Tile::Empty); // player
    g.set(3, 3, Tile::Empty); // lone enemy cell
    return Game(g);
}

} // namespace

TEST(EnemyTest, AddEnemyPlacesAtTileCentre)
{
    Game game = makeOpenRoom();
    ASSERT_TRUE(game.addEnemy(2, 3));
    ASSERT_EQ(game.enemies().size(), 1u);
    EXPECT_EQ(game.enemies().front()->subX(), 2 * kSubcell);
    EXPECT_EQ(game.enemies().front()->subY(), 3 * kSubcell);
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
    Game game = makeRoamRoom();
    ASSERT_TRUE(game.addEnemy(3, 3));

    bool leftStart = false;
    for (int i = 0; i < 2000 && !leftStart; ++i) {
        game.update(16);
        const IEnemy &e = *game.enemies().front();
        if (e.subX() / kSubcell != 3 || e.subY() / kSubcell != 3)
            leftStart = true;
    }
    EXPECT_TRUE(leftStart);
}

TEST(EnemyTest, BoxedEnemyStaysPut)
{
    Game game = makeBoxedRoom();
    ASSERT_TRUE(game.addEnemy(3, 3));
    for (int i = 0; i < 100; ++i)
        game.update(16);
    EXPECT_EQ(game.enemies().front()->subX(), 3 * kSubcell);
    EXPECT_EQ(game.enemies().front()->subY(), 3 * kSubcell);
}

TEST(EnemyTest, StaysGridLocked)
{
    Game game = makeRoamRoom();
    ASSERT_TRUE(game.addEnemy(3, 3));
    // Grid-locked movement only ever travels along one axis at a time, so the
    // other axis is always exactly on a tile line — never a diagonal drift.
    for (int i = 0; i < 1000; ++i) {
        game.update(16);
        const IEnemy &e = *game.enemies().front();
        EXPECT_TRUE(e.subX() % kSubcell == 0 || e.subY() % kSubcell == 0)
            << "off-grid at step " << i << ": " << e.subX() << "," << e.subY();
    }
}

TEST(EnemyTest, MovementIsDeterministic)
{
    Game a = makeRoamRoom();
    Game b = makeRoamRoom();
    ASSERT_TRUE(a.addEnemy(3, 3));
    ASSERT_TRUE(b.addEnemy(3, 3));
    for (int i = 0; i < 500; ++i) {
        a.update(16);
        b.update(16);
        EXPECT_EQ(a.enemies().front()->subX(), b.enemies().front()->subX());
        EXPECT_EQ(a.enemies().front()->subY(), b.enemies().front()->subY());
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
    for (const auto &e : game.enemies())
    {
        const int tx = e->subX() / kSubcell;
        const int ty = e->subY() / kSubcell;
        EXPECT_GE(std::abs(tx - 1) + std::abs(ty - 1), 4);
        EXPECT_EQ(game.grid().at(tx, ty), Tile::Empty);
    }
}

TEST(EnemyTest, ArenaSpawnedEnemiesCanMove)
{
    // Every seeded enemy must have at least one empty orthogonal neighbour, so it
    // can actually roam instead of spawning frozen inside a brick pocket.
    for (std::uint64_t seed = 1; seed <= 8; ++seed)
    {
        Game game(13, 11, seed);
        const Grid &g = game.grid();
        for (const auto &e : game.enemies())
        {
            const int tx = e->subX() / kSubcell;
            const int ty = e->subY() / kSubcell;
            const bool canMove =
                (g.inBounds(tx - 1, ty) && g.at(tx - 1, ty) == Tile::Empty) ||
                (g.inBounds(tx + 1, ty) && g.at(tx + 1, ty) == Tile::Empty) ||
                (g.inBounds(tx, ty - 1) && g.at(tx, ty - 1) == Tile::Empty) ||
                (g.inBounds(tx, ty + 1) && g.at(tx, ty + 1) == Tile::Empty);
            EXPECT_TRUE(canMove) << "frozen enemy at (" << tx << "," << ty
                                 << ") seed " << seed;
        }
    }
}

TEST(EnemyTest, ArenaSpawnIsDeterministic)
{
    Game a(13, 11, 7);
    Game b(13, 11, 7);
    ASSERT_EQ(a.enemies().size(), b.enemies().size());
    for (std::size_t i = 0; i < a.enemies().size(); ++i)
    {
        EXPECT_EQ(a.enemies()[i]->subX(), b.enemies()[i]->subX());
        EXPECT_EQ(a.enemies()[i]->subY(), b.enemies()[i]->subY());
    }
}

TEST(EnemyTest, ChaserHomesInOnThePlayer)
{
    // The player sits still in its (1,1) pocket; a Chaser dropped in the far corner
    // of the open room must walk it down and catch it (a contact loss).
    Game game = makeOpenRoom();
    ASSERT_TRUE(game.addEnemy(3, 3, EnemyType::Chaser));

    bool caught = false;
    for (int i = 0; i < 2000 && !caught; ++i)
    {
        game.update(16);
        if (game.state() == GameState::Lost)
            caught = true;
    }
    EXPECT_TRUE(caught);
}

TEST(EnemyTest, ChaserClosesTheDistance)
{
    // Independent of the catch: each committed step should never move it further
    // from the player, and overall it must get closer than where it started.
    Game game = makeOpenRoom();
    ASSERT_TRUE(game.addEnemy(3, 3, EnemyType::Chaser));

    const auto dist = [&]
    {
        const IEnemy &e = *game.enemies().front();
        return std::abs(e.subX() / kSubcell - game.playerX())
             + std::abs(e.subY() / kSubcell - game.playerY());
    };

    const int start = dist();
    int prev = start;
    for (int i = 0; i < 50 && game.state() == GameState::Playing; ++i)
    {
        game.update(16);
        EXPECT_LE(dist(), prev);
        prev = dist();
    }
    EXPECT_LT(prev, start);
}

TEST(EnemyTest, ChaserRoamsWhenWalledOffFromPlayer)
{
    // The player is sealed in its pocket, unreachable. A Chaser must not freeze
    // against the wall between them: it falls back to roaming its own block, and
    // can never reach the player to end the run.
    Game game = makeRoamRoom();
    ASSERT_TRUE(game.addEnemy(4, 4, EnemyType::Chaser));

    bool leftStart = false;
    for (int i = 0; i < 2000; ++i)
    {
        game.update(16);
        const IEnemy &e = *game.enemies().front();
        if (e.subX() / kSubcell != 4 || e.subY() / kSubcell != 4)
            leftStart = true;
    }
    EXPECT_TRUE(leftStart);
    EXPECT_EQ(game.state(), GameState::Playing);
}

TEST(EnemyTest, ArenaSpawnsAMixOfArchetypes)
{
    // A generated arena seeds both hunters and roamers, so the variety actually
    // reaches the player (not just one type by accident of the draw).
    Game game(13, 11, 1);
    int chasers = 0;
    int wanderers = 0;
    for (const auto &e : game.enemies())
    {
        if (e->type() == EnemyType::Chaser)
            ++chasers;
        else
            ++wanderers;
    }
    EXPECT_GE(chasers, 1);
    EXPECT_GE(wanderers, 1);
}
