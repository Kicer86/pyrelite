
// In-run progression (M3): XP banks per kill (never from bricks), crossing a level
// threshold drops a cluster of perk crystals where the enemy fell, and walking onto
// one claims its perk while the rest vanish — no pause. Built on hand-carved grids
// (a bomb the player lights and retreats from) so XP and drops are deterministic.

#include "game/game.h"

#include <gtest/gtest.h>

#include "grid/grid.h"

using namespace pyrelite;

namespace {

// An all-wall grid; tests carve out only the cells they need.
Grid solid(int width, int height)
{
    Grid g(width, height);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            g.set(x, y, Tile::Wall);
    return g;
}

// A room the player can bomb a brick in and survive:
//   # # # # #
//   # P . B #   player (1,1), (2,1) open, brick (3,1)
//   # . # # #   (1,2) lets the player duck out of its own blast
//   # # # # #
Game makeBrickRoom()
{
    Grid g = solid(5, 4);
    g.set(1, 1, Tile::Empty);
    g.set(2, 1, Tile::Empty);
    g.set(3, 1, Tile::Brick);
    g.set(1, 2, Tile::Empty);
    return Game(g);
}

// Bomb the brick at (3,1) from (2,1) and retreat to (1,2), clear of the blast.
void bombTheBrick(Game &game)
{
    ASSERT_TRUE(game.tryMove(Direction::Right)); // to (2,1)
    ASSERT_TRUE(game.placeBomb());
    ASSERT_TRUE(game.tryMove(Direction::Left));  // back to (1,1)
    ASSERT_TRUE(game.tryMove(Direction::Down));  // to (1,2)
    game.update(2000);                           // detonate; brick falls
}

// A room where the player bombs the enemy at (3,1) from (2,1) and ducks to (1,2),
// plus a second enemy boxed at (5,1) that can never move or be reached — so the kill
// is never the last one and the run stays live, letting a level-up fire instead of
// an immediate Won. The kill at (3,1) leaves three reachable floor tiles in a line
// ((3,1)(2,1)(1,1)) for the perk cluster to land on.
//   # # # # # # #
//   # P . E # S #
//   # . # # # # #
//   # # # # # # #
Game makeKillRoomWithSurvivor()
{
    Grid g = solid(7, 4);
    g.set(1, 1, Tile::Empty);
    g.set(2, 1, Tile::Empty);
    g.set(3, 1, Tile::Empty);
    g.set(1, 2, Tile::Empty);
    g.set(5, 1, Tile::Empty); // boxed survivor pocket: every neighbour is a wall
    Game game(g);
    EXPECT_TRUE(game.addEnemy(3, 1));
    EXPECT_TRUE(game.addEnemy(5, 1));
    return game;
}

// Light the bomb at (2,1) to kill the enemy on (3,1); the bomb itself walls that
// enemy into its tile (its only exit), so it cannot escape before detonation.
void bombTheEnemy(Game &game)
{
    ASSERT_TRUE(game.tryMove(Direction::Right)); // to (2,1)
    ASSERT_TRUE(game.placeBomb());
    ASSERT_TRUE(game.tryMove(Direction::Left));  // back to (1,1)
    ASSERT_TRUE(game.tryMove(Direction::Down));  // to (1,2), clear
    game.update(2000);                           // detonate; enemy on (3,1) dies
}

const PerkCrystal *crystalAt(const Game &game, int x, int y)
{
    for (const PerkCrystal &crystal : game.perkCrystals())
        if (crystal.x == x && crystal.y == y)
            return &crystal;
    return nullptr;
}

} // namespace

TEST(ProgressionTest, StartsAtLevelOneWithNoXp)
{
    Game game = makeBrickRoom();
    EXPECT_EQ(game.level(), 1);
    EXPECT_EQ(game.xp(), 0);
    EXPECT_EQ(game.state(), GameState::Playing);
    EXPECT_TRUE(game.perkCrystals().empty());
    EXPECT_GT(game.xpToNextLevel(), 0);
}

TEST(ProgressionTest, ClearingABrickGrantsNoXp)
{
    Game game = makeBrickRoom();
    bombTheBrick(game);

    EXPECT_EQ(game.xp(), 0);     // progress comes from kills, not bricks
    EXPECT_EQ(game.level(), 1);
    EXPECT_TRUE(game.perkCrystals().empty());
    EXPECT_EQ(game.state(), GameState::Playing);
}

TEST(ProgressionTest, KillingAnEnemyLevelsUpAndDropsAPerkCluster)
{
    Game game = makeKillRoomWithSurvivor();
    bombTheEnemy(game);

    EXPECT_EQ(game.state(), GameState::Playing);  // the run never pauses for a level-up
    EXPECT_EQ(game.level(), 2);                   // levelled immediately on the kill
    EXPECT_EQ(game.perkCrystals().size(), 3u);    // a cluster dropped where it fell
    EXPECT_NE(crystalAt(game, 3, 1), nullptr);    // including on the dead enemy's tile
}

TEST(ProgressionTest, AClusterOffersDistinctPerks)
{
    Game game = makeKillRoomWithSurvivor();
    bombTheEnemy(game);

    const auto &crystals = game.perkCrystals();
    ASSERT_EQ(crystals.size(), 3u);
    for (std::size_t i = 0; i < crystals.size(); ++i)
        for (std::size_t j = i + 1; j < crystals.size(); ++j)
            EXPECT_NE(crystals[i].type, crystals[j].type); // drawn without replacement
}

TEST(ProgressionTest, ClaimingACrystalAppliesItAndClearsTheCluster)
{
    Game game = makeKillRoomWithSurvivor();
    bombTheEnemy(game);
    ASSERT_EQ(game.perkCrystals().size(), 3u);

    // The player at (1,2) will step up onto the crystal sitting at (1,1).
    const PerkCrystal *target = crystalAt(game, 1, 1);
    ASSERT_NE(target, nullptr);
    const PerkType picked = target->type;

    ASSERT_TRUE(game.tryMove(Direction::Up)); // onto (1,1): claim it

    EXPECT_TRUE(game.perkCrystals().empty()); // the rest of the cluster vanished
    switch (picked)
    {
    case PerkType::PierceBlast:
        EXPECT_TRUE(game.pierceBlast());
        break;
    case PerkType::Shield:
        EXPECT_EQ(game.shieldCharges(), 1);
        break;
    case PerkType::RemoteDetonator:
        EXPECT_TRUE(game.remoteDetonator());
        break;
    }
}

TEST(ProgressionTest, AClearingKillWinsRatherThanDroppingPerks)
{
    // The classic single-enemy kill: clearing the last enemy wins outright, even
    // though that same kill banks enough XP to level — Won takes priority over loot.
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

    bombTheEnemy(game);

    EXPECT_EQ(game.state(), GameState::Won);
    EXPECT_TRUE(game.enemies().empty());
    EXPECT_GE(game.xp(), game.xpToNextLevel()); // the threshold was met...
    EXPECT_EQ(game.level(), 1);                  // ...but no level / loot followed
    EXPECT_TRUE(game.perkCrystals().empty());
}

TEST(ProgressionTest, BankedXpDropsAnotherClusterAfterTheFirstIsClaimed)
{
    // A 1-wide gallery of three enemies that one big blast clears at once, banking
    // enough XP for two levels in a single tick. A boxed survivor keeps the run live.
    //   # # # # # # # #
    //   # B E E E # S #   bomb at (1,1); enemies (2,1)(3,1)(4,1); survivor (6,1)
    //   # . # # # # # #   (1,2) drops to the escape corridor below
    //   # . . . . # # #   (1,3)(2,3)(3,3)(4,3): a corner out of the column-1 down arm
    //   # # # # # # # #
    Grid g = solid(8, 5);
    for (int x = 1; x <= 4; ++x)
        g.set(x, 1, Tile::Empty);
    g.set(6, 1, Tile::Empty);     // boxed survivor
    g.set(1, 2, Tile::Empty);     // step down out of the right-arm row
    for (int x = 1; x <= 4; ++x)
        g.set(x, 3, Tile::Empty); // corner refuge, walled off from the enemy row
    Game game(g);
    game.setBombRange(3);         // reach all three enemies and stop at the (5,1) wall
    ASSERT_TRUE(game.addEnemy(2, 1));
    ASSERT_TRUE(game.addEnemy(3, 1));
    ASSERT_TRUE(game.addEnemy(4, 1));
    ASSERT_TRUE(game.addEnemy(6, 1));

    ASSERT_TRUE(game.placeBomb());               // bomb on the spawn (1,1)
    ASSERT_TRUE(game.tryMove(Direction::Down));  // (1,2)
    ASSERT_TRUE(game.tryMove(Direction::Down));  // (1,3)
    ASSERT_TRUE(game.tryMove(Direction::Right)); // (2,3): around the corner, safe
    game.update(2000);                           // detonate; all three enemies die

    ASSERT_EQ(game.level(), 2);                  // first of the two banked levels
    ASSERT_FALSE(game.perkCrystals().empty());   // its cluster is on the floor
    EXPECT_EQ(game.enemies().size(), 1u);        // only the survivor remains

    game.update(500);          // let the blast's flames die before walking back in
    ASSERT_EQ(game.level(), 2); // the unclaimed cluster guards the next level meanwhile

    // Walk back to the enemy row and slide along it onto a crystal (the cluster sits
    // somewhere in (1,1)..(4,1), wherever the first enemy fell), claiming it.
    ASSERT_TRUE(game.tryMove(Direction::Left));  // (1,3)
    ASSERT_TRUE(game.tryMove(Direction::Up));    // (1,2)
    ASSERT_TRUE(game.tryMove(Direction::Up));    // (1,1): claims if a crystal is here
    for (int i = 0; i < 3 && !game.perkCrystals().empty(); ++i)
        ASSERT_TRUE(game.tryMove(Direction::Right));
    ASSERT_TRUE(game.perkCrystals().empty());    // first cluster claimed

    game.update(16); // settle: the second banked level now drops its own cluster
    EXPECT_EQ(game.level(), 3);
    EXPECT_FALSE(game.perkCrystals().empty());
}
