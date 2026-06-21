
#include "game.h"

#include <cstdint>
#include <cstdlib>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "chunk.h"
#include "grid.h"
#include "movement.h"

using namespace pyrelite;

TEST(StreamedGameTest, SpawnsPlayerAtOriginPocket)
{
    Game game(1, Game::Streamed{});
    EXPECT_EQ(game.playerX(), 1);
    EXPECT_EQ(game.playerY(), 1);
    EXPECT_EQ(game.state(), GameState::Playing);
    EXPECT_EQ(game.tileAt(1, 1), Tile::Empty);
}

TEST(StreamedGameTest, SeedsStarterEnemiesNearOrigin)
{
    for (std::uint64_t seed = 1; seed <= 8; ++seed)
    {
        Game game(seed, Game::Streamed{});
        ASSERT_EQ(game.enemies().size(), 5u) << "seed " << seed;
        for (const auto &enemy : game.enemies())
        {
            EXPECT_GE(enemy->tileX(), 0);
            EXPECT_GE(enemy->tileY(), 0);
            EXPECT_LT(enemy->tileX(), 24);
            EXPECT_LT(enemy->tileY(), 24);
            // A safe distance from the player pocket, so the opening is not a death trap.
            EXPECT_GE(std::abs(enemy->tileX() - 1) + std::abs(enemy->tileY() - 1), 4);
        }
    }
}

TEST(StreamedGameTest, PlayerStepsThroughTheSpawnPocket)
{
    Game game(1, Game::Streamed{});
    game.setMoveDirection(Direction::Right);
    // (2, 1) is part of the guaranteed-clear spawn pocket; one tile crosses in ~341 ms.
    for (int i = 0; i < 40 && game.playerX() == 1; ++i)
        game.update(16);
    EXPECT_EQ(game.playerX(), 2);
}

TEST(StreamedGameTest, RemoteEnemiesAreNotSimulatedOutsideTheActiveWindow)
{
    Game game(1, Game::Streamed{});
    EXPECT_FALSE(game.walkable(3 * kChunkSize, 0));
    for (int i = 0; i < 7; ++i)
        ASSERT_TRUE(game.tryMove(Direction::Down));
    for (int i = 0; i < 80; ++i)
        ASSERT_TRUE(game.tryMove(Direction::Right));
    EXPECT_FALSE(game.walkable(0, 0));

    std::vector<std::pair<int, int>> before;
    for (const auto &enemy : game.enemies())
        before.emplace_back(enemy->subX(), enemy->subY());

    game.update(16);

    std::vector<std::pair<int, int>> after;
    for (const auto &enemy : game.enemies())
        after.emplace_back(enemy->subX(), enemy->subY());
    EXPECT_EQ(after, before);
}

TEST(StreamedGameTest, EndlessObjectiveDoesNotWinWhenTheEnemyRosterIsCleared)
{
    Grid grid(7, 5);
    grid.set(3, 0, Tile::Wall);
    grid.set(3, 2, Tile::Wall);
    grid.set(4, 1, Tile::Wall);
    Game game(std::move(grid), 1, Game::Objective::Endless);
    ASSERT_TRUE(game.addEnemy(3, 1));
    game.setBombRange(2);
    ASSERT_TRUE(game.placeBomb());
    ASSERT_TRUE(game.tryMove(Direction::Down));
    ASSERT_TRUE(game.tryMove(Direction::Right));

    game.update(2000);

    EXPECT_TRUE(game.enemies().empty());
    EXPECT_EQ(game.state(), GameState::Playing);
}
