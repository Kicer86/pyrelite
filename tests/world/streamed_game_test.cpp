
#include "game/game.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <optional>
#include <queue>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "world/chunk.h"
#include "world/world.h"
#include "grid/grid.h"
#include "grid/movement.h"

namespace
{
    pyrelite::Direction opposite(pyrelite::Direction d)
    {
        switch (d)
        {
        case pyrelite::Direction::Up:    return pyrelite::Direction::Down;
        case pyrelite::Direction::Down:  return pyrelite::Direction::Up;
        case pyrelite::Direction::Left:  return pyrelite::Direction::Right;
        case pyrelite::Direction::Right: return pyrelite::Direction::Left;
        }
        return d;
    }

    // Walk the player east along the (connected, winding) channel until its chunk is at
    // least `targetChunkX` — a DFS over walkable tiles that physically backtracks, so it
    // copes with whatever shape the channel takes. Returns whether it got there.
    bool walkEastTo(pyrelite::Game &game, int targetChunkX)
    {
        using pyrelite::Direction;
        const auto here = [&] { return std::pair{game.playerX(), game.playerY()}; };
        std::set<std::pair<int, int>> visited{here()};
        std::vector<Direction> trail;
        const Direction order[] = {Direction::Right, Direction::Down,
                                   Direction::Up, Direction::Left};
        for (int guard = 0; guard < 50000; ++guard)
        {
            if (pyrelite::World::chunkCoord(game.playerX()) >= targetChunkX)
                return true;
            bool advanced = false;
            for (Direction d : order)
            {
                if (!game.tryMove(d))
                    continue;
                if (visited.insert(here()).second)
                {
                    trail.push_back(d);
                    advanced = true;
                    break;
                }
                game.tryMove(opposite(d)); // already seen: step back and try another
            }
            if (!advanced)
            {
                if (trail.empty())
                    return false;
                game.tryMove(opposite(trail.back()));
                trail.pop_back();
            }
        }
        return pyrelite::World::chunkCoord(game.playerX()) >= targetChunkX;
    }

    // BFS over Empty tiles to (tx, ty), returning the directions to walk there (empty if
    // unreachable within a bounded box). The walk uses tryMove only, which never ticks
    // enemies, so they stay frozen on their spawn cells while the player is positioned.
    std::vector<pyrelite::Direction> pathTo(pyrelite::Game &game, int tx, int ty)
    {
        using pyrelite::Direction;
        const int sx = game.playerX();
        const int sy = game.playerY();
        std::map<std::pair<int, int>, std::pair<int, int>> parent;
        std::map<std::pair<int, int>, Direction> via;
        std::queue<std::pair<int, int>> queue;
        queue.push({sx, sy});
        parent[{sx, sy}] = {sx, sy};
        const Direction dirs[] = {Direction::Right, Direction::Left,
                                  Direction::Down, Direction::Up};
        const int dx[] = {1, -1, 0, 0};
        const int dy[] = {0, 0, 1, -1};
        constexpr int kBox = 160;
        while (!queue.empty())
        {
            const auto [cx, cy] = queue.front();
            queue.pop();
            if (cx == tx && cy == ty)
                break;
            for (int i = 0; i < 4; ++i)
            {
                const int nx = cx + dx[i];
                const int ny = cy + dy[i];
                if (std::abs(nx - sx) > kBox || std::abs(ny - sy) > kBox)
                    continue;
                if (game.tileAt(nx, ny) != pyrelite::Tile::Empty)
                    continue;
                if (parent.count({nx, ny}))
                    continue;
                parent[{nx, ny}] = {cx, cy};
                via[{nx, ny}] = dirs[i];
                queue.push({nx, ny});
            }
        }
        std::vector<Direction> path;
        if (parent.count({tx, ty}))
        {
            std::pair<int, int> cur{tx, ty};
            while (cur != std::make_pair(sx, sy))
            {
                path.push_back(via[cur]);
                cur = parent[cur];
            }
            std::reverse(path.begin(), path.end());
        }
        return path;
    }

    bool navigateTo(pyrelite::Game &game, int tx, int ty)
    {
        if (game.playerX() == tx && game.playerY() == ty)
            return true;
        const auto path = pathTo(game, tx, ty);
        if (path.empty())
            return false;
        for (pyrelite::Direction d : path)
            if (!game.tryMove(d))
                return false;
        return game.playerX() == tx && game.playerY() == ty;
    }

    // Deterministically kill exactly one streamed enemy without losing the run: find a
    // roster enemy boxed into a dead-end (a single open neighbour), bomb that one exit so
    // it cannot dodge, retreat off the blast cross, then detonate. Returns the killed
    // enemy's tile, or nullopt if no trappable enemy is reachable for this seed.
    std::optional<std::pair<int, int>> trapKillOneEnemy(pyrelite::Game &game)
    {
        using pyrelite::Tile;
        const int dx[] = {1, -1, 0, 0};
        const int dy[] = {0, 0, 1, -1};
        for (const auto &enemy : game.enemies())
        {
            const int ex = enemy->tileX();
            const int ey = enemy->tileY();
            int open = 0;
            int nx = 0;
            int ny = 0;
            for (int i = 0; i < 4; ++i)
                if (game.tileAt(ex + dx[i], ey + dy[i]) == Tile::Empty)
                {
                    ++open;
                    nx = ex + dx[i];
                    ny = ey + dy[i];
                }
            if (open != 1)
                continue; // not a dead-end — it could step out of the blast

            if (!navigateTo(game, nx, ny) || !game.placeBomb())
                continue;

            // Retreat to any reachable Empty tile off BOTH blast axes (never on the cross,
            // so it is safe whatever the range). The bomb on the enemy's only exit keeps
            // it boxed while we leave.
            std::optional<std::pair<int, int>> safe;
            for (int radius = 2; radius <= 9 && !safe; ++radius)
                for (int oy = -radius; oy <= radius && !safe; ++oy)
                    for (int ox = -radius; ox <= radius && !safe; ++ox)
                    {
                        const int tx = nx + ox;
                        const int ty = ny + oy;
                        if (tx == nx || ty == ny)
                            continue;
                        if (game.tileAt(tx, ty) != Tile::Empty)
                            continue;
                        if (!pathTo(game, tx, ty).empty())
                            safe = {tx, ty};
                    }
            if (!safe || !navigateTo(game, safe->first, safe->second))
                return std::nullopt;

            const std::size_t before = game.killedEnemyCount();
            for (int t = 0; t < 300 && game.state() == pyrelite::GameState::Playing; ++t)
                game.update(16);
            if (game.killedEnemyCount() > before
                && game.state() == pyrelite::GameState::Playing)
                return std::pair{ex, ey};
            return std::nullopt;
        }
        return std::nullopt;
    }
}

using namespace pyrelite;

TEST(StreamedGameTest, SpawnsPlayerAtOriginPocket)
{
    Game game(1, Game::Streamed{});
    EXPECT_EQ(game.playerX(), 1);
    EXPECT_EQ(game.playerY(), 1);
    EXPECT_EQ(game.state(), GameState::Playing);
    EXPECT_EQ(game.tileAt(1, 1), Tile::Empty);
}

TEST(StreamedGameTest, SeedsZoneEnemiesAroundOriginAtASafeDistance)
{
    // The streamed game no longer scatters a fixed five enemies; it activates the zones
    // around the spawn and seeds each from its roster. The origin remains populated, and
    // no enemy starts close enough to the pocket to make the opening a death trap.
    for (std::uint64_t seed = 1; seed <= 8; ++seed)
    {
        Game game(seed, Game::Streamed{});
        EXPECT_FALSE(game.enemies().empty()) << "seed " << seed;
        for (const auto &enemy : game.enemies())
            EXPECT_GE(std::abs(enemy->tileX() - 1) + std::abs(enemy->tileY() - 1), 4)
                << "seed " << seed;
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

TEST(StreamedGameTest, TravellingAwayEvictsDistantZoneEnemies)
{
    Game game(1, Game::Streamed{});
    ASSERT_FALSE(game.enemies().empty());
    EXPECT_FALSE(game.walkable(3 * kChunkSize, 0));

    // Travel far past the origin zone (its tiles span x in [-32, 31]). Its chunks leave
    // the active window, so the zone is evicted: no enemy lingers back near the origin,
    // and a further tick neither moves nor resurrects anything there.
    ASSERT_TRUE(walkEastTo(game, 12));
    EXPECT_FALSE(game.walkable(0, 0));

    for (const auto &enemy : game.enemies())
        EXPECT_GT(enemy->tileX(), kZoneSize) << "stale enemy near origin";

    game.update(16);
    for (const auto &enemy : game.enemies())
        EXPECT_GT(enemy->tileX(), kZoneSize);
}

TEST(StreamedGameTest, KilledZoneEnemiesDoNotRespawnOnRevisit)
{
    // Find a seed with a trappable (dead-end) start enemy, kill exactly one, then unload
    // and reload its zone by walking far out and navigating back to the kill site. The
    // zone must repopulate from its roster — yet never resurrect the enemy we killed.
    bool exercised = false;
    for (std::uint64_t seed = 1; seed <= 30 && !exercised; ++seed)
    {
        Game game(seed, Game::Streamed{});
        const auto killed = trapKillOneEnemy(game);
        if (!killed)
            continue;
        const std::size_t deadAfterKill = game.killedEnemyCount();
        EXPECT_GE(deadAfterKill, 1u) << "seed " << seed;
        const int homeX = game.playerX();
        const int homeY = game.playerY();

        // Travel past every start zone so the kill's zone unloads, then walk back to the
        // exact kill site so it reloads under the player. Skip seeds whose winding channel
        // defeats the bounded navigation helpers; one seed that round-trips is enough to
        // prove the anti-farm guarantee.
        if (!walkEastTo(game, 8) || !navigateTo(game, homeX, homeY))
            continue;
        exercised = true;

        EXPECT_EQ(game.killedEnemyCount(), deadAfterKill) << "seed " << seed; // never reset
        EXPECT_FALSE(game.enemies().empty()) << "seed " << seed;              // repopulated
        for (const auto &enemy : game.enemies())
            EXPECT_FALSE(enemy->tileX() == killed->first
                         && enemy->tileY() == killed->second)
                << "seed " << seed << ": resurrected at "
                << killed->first << "," << killed->second;
    }
    EXPECT_TRUE(exercised) << "no trappable start enemy round-tripped across the scanned seeds";
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
