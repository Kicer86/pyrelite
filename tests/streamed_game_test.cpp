
#include "game.h"

#include <cstdint>
#include <cstdlib>

#include <gtest/gtest.h>

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
    Game game(1, Game::Streamed{});
    EXPECT_GT(game.enemies().size(), 0u);
    EXPECT_LE(game.enemies().size(), 5u);
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

TEST(StreamedGameTest, PlayerStepsThroughTheSpawnPocket)
{
    Game game(1, Game::Streamed{});
    game.setMoveDirection(Direction::Right);
    // (2, 1) is part of the guaranteed-clear spawn pocket; one tile crosses in ~341 ms.
    for (int i = 0; i < 40 && game.playerX() == 1; ++i)
        game.update(16);
    EXPECT_EQ(game.playerX(), 2);
}

TEST(StreamedGameTest, EndlessRunNeverReportsWon)
{
    // There is no "clear all enemies" in an unbounded world, so the run never wins —
    // it only ever stays Playing or ends in Lost.
    Game game(1, Game::Streamed{});
    for (int i = 0; i < 200; ++i)
    {
        game.update(16);
        ASSERT_NE(game.state(), GameState::Won);
    }
}
