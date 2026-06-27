
#include "game/game.h"

#include <array>

#include <gtest/gtest.h>

#include "grid/grid.h"

using namespace pyrelite;

namespace
{
    Grid roomWithBrickGrid()
    {
        Grid g(7, 7);
        for (int i = 0; i < 7; ++i)
        {
            g.set(i, 0, Tile::Wall);
            g.set(i, 6, Tile::Wall);
            g.set(0, i, Tile::Wall);
            g.set(6, i, Tile::Wall);
        }
        g.set(3, 1, Tile::Brick);
        return g;
    }

    Game makeRoomWithBrick(std::uint64_t seed = 1)
    {
        Game game(roomWithBrickGrid(), seed);
        game.setPowerUpDropPercent(100); // deterministic drop for the drop/effect tests
        return game;
    }

    void destroyBrick(Game &game)
    {
        ASSERT_TRUE(game.placeBomb());
        EXPECT_TRUE(game.update(2000));
    }

    void collectDroppedPowerUp(Game &game)
    {
        ASSERT_TRUE(game.tryMove(Direction::Right));
        ASSERT_TRUE(game.tryMove(Direction::Right));
    }
}

TEST(PowerUpTest, DestroyedBrickDropsPowerUp)
{
    Game game = makeRoomWithBrick(); // forced to 100% by the helper
    destroyBrick(game);

    ASSERT_EQ(game.powerUps().size(), 1u);
    EXPECT_EQ(game.powerUps()[0].x, 3);
    EXPECT_EQ(game.powerUps()[0].y, 1);
    EXPECT_TRUE(game.hasPowerUpAt(3, 1));
}

TEST(PowerUpTest, BrickDropsAreGatedByChance)
{
    {
        Game game = makeRoomWithBrick();
        game.setPowerUpDropPercent(0); // never drops
        destroyBrick(game);
        EXPECT_TRUE(game.powerUps().empty());
    }
    {
        Game game = makeRoomWithBrick();
        game.setPowerUpDropPercent(100); // always drops
        destroyBrick(game);
        EXPECT_EQ(game.powerUps().size(), 1u);
    }
}

TEST(PowerUpTest, DefaultDropRateIsPartial)
{
    // At the shipped default rate a brick neither always nor never drops: counted over
    // a span of seeds, some destructions yield a power-up and some do not.
    int drops = 0;
    int trials = 0;
    for (std::uint64_t seed = 1; seed <= 60; ++seed)
    {
        Game game(roomWithBrickGrid(), seed); // default drop rate, no override
        EXPECT_EQ(game.powerUpDropPercent(), 30);
        destroyBrick(game);
        ++trials;
        if (!game.powerUps().empty())
            ++drops;
    }
    EXPECT_GT(drops, 0);
    EXPECT_LT(drops, trials);
}

TEST(PowerUpTest, CollectedPowerUpIsRemovedAndAppliesEffect)
{
    std::array<bool, 3> seen = {};

    for (std::uint64_t seed = 1; seed < 30; ++seed)
    {
        Game game = makeRoomWithBrick(seed);
        destroyBrick(game);
        ASSERT_EQ(game.powerUps().size(), 1u);

        const PowerUpType type = game.powerUps()[0].type;
        const int bombLimit = game.bombLimit();
        const int bombRange = game.bombRange();
        const int playerSpeed = game.playerSpeed();

        collectDroppedPowerUp(game);

        EXPECT_FALSE(game.hasPowerUpAt(3, 1));
        switch (type)
        {
        case PowerUpType::BombLimit:
            EXPECT_EQ(game.bombLimit(), bombLimit + 1);
            seen[0] = true;
            break;
        case PowerUpType::BombRange:
            EXPECT_EQ(game.bombRange(), bombRange + 1);
            seen[1] = true;
            break;
        case PowerUpType::Speed:
            EXPECT_EQ(game.playerSpeed(), playerSpeed + 1);
            seen[2] = true;
            break;
        }
    }

    EXPECT_TRUE(seen[0]);
    EXPECT_TRUE(seen[1]);
    EXPECT_TRUE(seen[2]);
}
