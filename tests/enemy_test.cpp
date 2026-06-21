
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

// A U-shaped corridor whose only route to the player at (1,1) first leads AWAY from
// them: down the right arm, across the bottom, up the left arm. A greedy chaser keeps
// stepping toward the player, hits the dividing wall and oscillates at the dead end; a
// BFS hunter follows the snake the whole way around. Enemy starts at (3,1).
Game makeUMaze()
{
    Grid g(7, 7);
    for (int y = 0; y < 7; ++y)
        for (int x = 0; x < 7; ++x)
            g.set(x, y, Tile::Wall);
    for (const auto &[x, y] : {std::pair{1, 1}, {1, 2}, {1, 3},
             {2, 3}, {3, 3}, {3, 2}, {3, 1}})
        g.set(x, y, Tile::Empty);
    return Game(g);
}

// A solid-walled 5x5 split down the middle by a column of BRICKS: the player pocket on
// the left, the enemy block on the right, joined only through the destructible bricks.
// A normal enemy is sealed off; a Ghost phases across. Player at (1,1), enemy at (3,3).
Game makeBrickSplitRoom()
{
    Grid g(5, 5);
    for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 5; ++x)
            g.set(x, y, Tile::Wall);
    for (int y = 1; y <= 3; ++y)
    {
        g.set(1, y, Tile::Empty); // player's left column
        g.set(2, y, Tile::Brick); // the brick divider
        g.set(3, y, Tile::Empty); // enemy's right column
    }
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
        EXPECT_EQ(game.tileAt(tx, ty), Tile::Empty);
    }
}

TEST(EnemyTest, ArenaSpawnedEnemiesCanMove)
{
    // Every seeded enemy must have at least one empty orthogonal neighbour, so it
    // can actually roam instead of spawning frozen inside a brick pocket.
    for (std::uint64_t seed = 1; seed <= 8; ++seed)
    {
        Game game(13, 11, seed);
        for (const auto &e : game.enemies())
        {
            const int tx = e->subX() / kSubcell;
            const int ty = e->subY() / kSubcell;
            const bool canMove =
                (game.tileAt(tx - 1, ty) == Tile::Empty) ||
                (game.tileAt(tx + 1, ty) == Tile::Empty) ||
                (game.tileAt(tx, ty - 1) == Tile::Empty) ||
                (game.tileAt(tx, ty + 1) == Tile::Empty);
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

TEST(EnemyTest, BouncerSweepsACorridorEndToEnd)
{
    // A 1-wide vertical corridor, walled off from the player pocket. A Bouncer dropped
    // in the middle must ricochet off both ends — reaching the top AND the bottom over
    // time — instead of walking one way and freezing.
    Grid g(7, 7);
    for (int y = 0; y < 7; ++y)
        for (int x = 0; x < 7; ++x)
            g.set(x, y, Tile::Wall);
    g.set(1, 1, Tile::Empty);         // player pocket, walled off
    for (int y = 2; y <= 5; ++y)
        g.set(3, y, Tile::Empty);     // the corridor
    Game game(g);
    ASSERT_TRUE(game.addEnemy(3, 3, EnemyType::Bouncer));

    bool reachedTop = false;
    bool reachedBottom = false;
    for (int i = 0; i < 4000 && !(reachedTop && reachedBottom); ++i)
    {
        game.update(16);
        const IEnemy &e = *game.enemies().front();
        if (e.tileX() == 3 && e.tileY() == 2)
            reachedTop = true;
        if (e.tileX() == 3 && e.tileY() == 5)
            reachedBottom = true;
    }
    EXPECT_TRUE(reachedTop);
    EXPECT_TRUE(reachedBottom);
    EXPECT_EQ(game.state(), GameState::Playing); // never reaches the walled-off player
}

TEST(EnemyTest, HunterPathfindsThroughAMazeToCatchThePlayer)
{
    // The BFS hunter rounds the U-corridor and runs the player down (a contact loss),
    // even though the route starts by moving away from them.
    Game game = makeUMaze();
    ASSERT_TRUE(game.addEnemy(3, 1, EnemyType::Hunter));

    bool caught = false;
    for (int i = 0; i < 4000 && !caught; ++i)
    {
        game.update(16);
        if (game.state() == GameState::Lost)
            caught = true;
    }
    EXPECT_TRUE(caught);
}

TEST(EnemyTest, GreedyChaserStallsInTheMazeTheHunterSolves)
{
    // The same maze defeats the greedy Chaser: it can only ever step toward the player,
    // so it oscillates at the dividing wall and never reaches them. This is exactly the
    // gap the Hunter's pathfinding fills.
    Game game = makeUMaze();
    ASSERT_TRUE(game.addEnemy(3, 1, EnemyType::Chaser));

    for (int i = 0; i < 4000; ++i)
        game.update(16);

    EXPECT_EQ(game.state(), GameState::Playing); // never caught the player
}

TEST(EnemyTest, GhostPhasesThroughBricksToCatchThePlayer)
{
    // The only route between the blocks is through the brick divider, which a Ghost
    // ignores: it crosses and runs the player down (a contact loss).
    Game game = makeBrickSplitRoom();
    ASSERT_TRUE(game.addEnemy(3, 3, EnemyType::Ghost));

    bool caught = false;
    for (int i = 0; i < 4000 && !caught; ++i)
    {
        game.update(16);
        if (game.state() == GameState::Lost)
            caught = true;
    }
    EXPECT_TRUE(caught);
}

TEST(EnemyTest, ChaserCannotCrossTheBrickWallTheGhostPhasesThrough)
{
    // The same divider seals a normal Chaser out: bricks are not walkable, the blocks
    // are otherwise solid-walled, so it never reaches the player. The contrast is the
    // Ghost's whole point — and the bricks stay intact (nothing destroyed them).
    Game game = makeBrickSplitRoom();
    ASSERT_TRUE(game.addEnemy(3, 3, EnemyType::Chaser));

    for (int i = 0; i < 4000; ++i)
        game.update(16);

    EXPECT_EQ(game.state(), GameState::Playing); // never crossed to the player
    EXPECT_EQ(game.tileAt(2, 2), Tile::Brick); // divider untouched
}

TEST(EnemyTest, ArenaSpawnsAMixOfArchetypes)
{
    // A generated arena seeds every archetype, so the variety actually reaches the
    // player (not just one type by accident of the draw).
    Game game(13, 11, 1);
    int chasers = 0;
    int bouncers = 0;
    int hunters = 0;
    int ghosts = 0;
    int wanderers = 0;
    for (const auto &e : game.enemies())
    {
        switch (e->type())
        {
        case EnemyType::Chaser:
            ++chasers;
            break;
        case EnemyType::Bouncer:
            ++bouncers;
            break;
        case EnemyType::Hunter:
            ++hunters;
            break;
        case EnemyType::Ghost:
            ++ghosts;
            break;
        case EnemyType::Wanderer:
            ++wanderers;
            break;
        }
    }
    EXPECT_GE(chasers, 1);
    EXPECT_GE(bouncers, 1);
    EXPECT_GE(hunters, 1);
    EXPECT_GE(ghosts, 1);
    EXPECT_GE(wanderers, 1);
}
