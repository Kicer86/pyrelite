
// In-run progression (M3): XP banks per kill and per brick, crossing a threshold
// opens a level-up that freezes the run until a perk is picked, and picking applies
// the perk's effect. Built on hand-carved grids (a bomb the player can light and
// retreat from) so XP gains are deterministic — no real arena, no Qt.

#include "game.h"

#include <gtest/gtest.h>

#include "grid.h"

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
// an immediate Won.
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

} // namespace

TEST(ProgressionTest, StartsAtLevelOneWithNoXp)
{
    Game game = makeBrickRoom();
    EXPECT_EQ(game.level(), 1);
    EXPECT_EQ(game.xp(), 0);
    EXPECT_EQ(game.state(), GameState::Playing);
    EXPECT_TRUE(game.perkChoices().empty());
    EXPECT_GT(game.xpToNextLevel(), 0);
}

TEST(ProgressionTest, ClearingABrickGrantsXpBelowTheThreshold)
{
    Game game = makeBrickRoom();
    bombTheBrick(game);

    EXPECT_GT(game.xp(), 0);                     // a cleared brick banks XP
    EXPECT_LT(game.xp(), game.xpToNextLevel());  // but a single brick is below a level
    EXPECT_EQ(game.level(), 1);
    EXPECT_EQ(game.state(), GameState::Playing);
}

TEST(ProgressionTest, KillingAnEnemyLevelsUpAndFreezesTheRun)
{
    Game game = makeKillRoomWithSurvivor();
    bombTheEnemy(game);

    ASSERT_EQ(game.state(), GameState::LevelUp);
    EXPECT_EQ(game.level(), 1);                       // not advanced until a perk is taken
    EXPECT_GE(game.xp(), game.xpToNextLevel());
    EXPECT_EQ(game.perkChoices().size(), 3u);
    EXPECT_FALSE(game.update(16));                    // simulation frozen during the offer
}

TEST(ProgressionTest, OfferedPerksAreDistinct)
{
    Game game = makeKillRoomWithSurvivor();
    bombTheEnemy(game);
    ASSERT_EQ(game.state(), GameState::LevelUp);

    const auto &choices = game.perkChoices();
    ASSERT_EQ(choices.size(), 3u);
    for (std::size_t i = 0; i < choices.size(); ++i)
        for (std::size_t j = i + 1; j < choices.size(); ++j)
            EXPECT_NE(choices[i], choices[j]); // drawn without replacement
}

TEST(ProgressionTest, ChoosingAPerkAppliesItsEffectAndResumes)
{
    Game game = makeKillRoomWithSurvivor();
    bombTheEnemy(game);
    ASSERT_EQ(game.state(), GameState::LevelUp);

    const PerkType picked = game.perkChoices().at(0);
    const int bombLimit = game.bombLimit();
    const int bombRange = game.bombRange();
    const int speed = game.playerSpeed();

    ASSERT_TRUE(game.choosePerk(0));

    EXPECT_EQ(game.level(), 2);
    EXPECT_EQ(game.state(), GameState::Playing);
    EXPECT_TRUE(game.perkChoices().empty());

    switch (picked)
    {
    case PerkType::ExtraBomb:
        EXPECT_EQ(game.bombLimit(), bombLimit + 1);
        break;
    case PerkType::BiggerBlast:
        EXPECT_EQ(game.bombRange(), bombRange + 1);
        break;
    case PerkType::SwiftFeet:
        EXPECT_EQ(game.playerSpeed(), speed + 1);
        break;
    }
}

TEST(ProgressionTest, ChoosePerkIsIgnoredOutsideALevelUp)
{
    Game game = makeBrickRoom(); // never leaves Playing
    EXPECT_FALSE(game.choosePerk(0));
    EXPECT_EQ(game.level(), 1);
    EXPECT_EQ(game.state(), GameState::Playing);
}

TEST(ProgressionTest, ChoosePerkRejectsAnOutOfRangeIndex)
{
    Game game = makeKillRoomWithSurvivor();
    bombTheEnemy(game);
    ASSERT_EQ(game.state(), GameState::LevelUp);

    EXPECT_FALSE(game.choosePerk(-1));
    EXPECT_FALSE(game.choosePerk(99));
    EXPECT_EQ(game.state(), GameState::LevelUp); // a bad pick leaves the offer open
    EXPECT_EQ(game.level(), 1);
}

TEST(ProgressionTest, BankedXpOpensConsecutiveLevelUps)
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

    ASSERT_EQ(game.state(), GameState::LevelUp); // first of the banked levels
    EXPECT_EQ(game.level(), 1);
    EXPECT_EQ(game.enemies().size(), 1u);        // only the survivor remains

    ASSERT_TRUE(game.choosePerk(0));
    EXPECT_EQ(game.level(), 2);
    EXPECT_EQ(game.state(), GameState::LevelUp); // still banked: a second offer opens

    ASSERT_TRUE(game.choosePerk(0));
    EXPECT_EQ(game.level(), 3);
    EXPECT_EQ(game.state(), GameState::Playing); // spent down; play resumes
}

TEST(ProgressionTest, WinningTakesPriorityOverALevelUp)
{
    // The classic single-enemy kill: clearing the last enemy wins outright, even
    // though that same kill banks enough XP to level — Won is not interrupted.
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
    EXPECT_EQ(game.level(), 1);                  // ...but no perk was offered
    EXPECT_TRUE(game.perkChoices().empty());
}
