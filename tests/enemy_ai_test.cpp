
// Enemy AI exercised in isolation: against a fake world and a scripted RNG, with
// NO real Game. Note the includes — chaser/wanderer + the interfaces, never game.h —
// which is exactly what the IGame/IRng seam buys us.

#include "chaser.h"
#include "igame.h"
#include "irng.h"
#include "wanderer.h"

#include <cstdint>
#include <set>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

using namespace pyrelite;

namespace {

// A hand-built IGame: every tile is walkable unless explicitly blocked, and the
// player sits at a fixed tile. No grid, no bombs, no Game.
class FakeWorld : public IGame
{
public:
    FakeWorld(int playerTileX, int playerTileY)
        : m_px(playerTileX), m_py(playerTileY) {}

    void block(int x, int y) { m_blocked.emplace(x, y); }

    bool walkable(int x, int y) const override
    {
        return !m_blocked.contains({x, y});
    }
    int playerX() const override { return m_px; }
    int playerY() const override { return m_py; }

private:
    int m_px;
    int m_py;
    std::set<std::pair<int, int>> m_blocked;
};

// An IRng that replays a fixed list of below() results and counts every draw, so a
// test can both steer a turn and assert the RNG was (or was not) consulted.
class ScriptedRng : public IRng
{
public:
    ScriptedRng() = default;
    explicit ScriptedRng(std::vector<std::uint32_t> belows)
        : m_belows(std::move(belows)) {}

    std::uint64_t next() override { ++m_calls; return 0; }
    std::uint32_t below(std::uint32_t bound) override
    {
        ++m_calls;
        const std::uint32_t v = m_idx < m_belows.size() ? m_belows[m_idx++] : 0;
        return v % bound;
    }
    bool chance(std::uint32_t) override { ++m_calls; return false; }

    int calls() const { return m_calls; }

private:
    std::vector<std::uint32_t> m_belows;
    std::size_t m_idx = 0;
    int m_calls = 0;
};

} // namespace

TEST(EnemyAiTest, ChaserHomesTowardThePlayerWithNoGame)
{
    FakeWorld world(10, 5); // player far to the right, clear path
    ScriptedRng rng;
    Chaser chaser(2, 5);

    bool reached = false;
    for (int i = 0; i < 2000 && !reached; ++i)
    {
        chaser.integrate(world, rng, 16);
        if (chaser.tileX() == 10 && chaser.tileY() == 5)
            reached = true;
    }
    EXPECT_TRUE(reached);
    EXPECT_EQ(rng.calls(), 0); // a greedy step toward the player never needs the RNG
}

TEST(EnemyAiTest, ChaserFallsBackToScriptedRngWhenWalledOff)
{
    FakeWorld world(9, 9); // player down-right, but both approaches are walled
    world.block(2, 1);     // Right (toward the player on x)
    world.block(1, 2);     // Down (toward the player on y)
    // Up (1,0) and Left (0,1) remain -> gathered as options[0], options[1].
    ScriptedRng rng({1}); // below(2) -> 1 -> Left
    Chaser chaser(1, 1);

    for (int i = 0; i < 40; ++i)
        chaser.integrate(world, rng, 16);

    EXPECT_EQ(chaser.tileX(), 0); // roamed Left per the script
    EXPECT_EQ(chaser.tileY(), 1);
    EXPECT_EQ(rng.calls(), 1);    // consulted exactly once, when boxed toward the player
}

TEST(EnemyAiTest, WandererKeepsHeadingWhileClearWithoutRng)
{
    FakeWorld world(0, 0); // a wanderer ignores the player
    ScriptedRng rng;
    Wanderer w(5, 5);      // default heading is Down, everything open

    const int startTileY = w.tileY();
    for (int i = 0; i < 200; ++i)
        w.integrate(world, rng, 16);

    EXPECT_GT(w.tileY(), startTileY); // travelled straight down
    EXPECT_EQ(w.tileX(), 5);          // never left its lane
    EXPECT_EQ(rng.calls(), 0);        // a clear path ahead never consults the RNG
}

TEST(EnemyAiTest, WandererTurnsToTheScriptedDirectionWhenBlocked)
{
    FakeWorld world(0, 0);
    world.block(5, 6); // straight ahead (Down) is a wall
    world.block(5, 4); // and Up
    // Left (4,5) and Right (6,5) remain -> options[0], options[1].
    ScriptedRng rng({1}); // below(2) -> 1 -> Right
    Wanderer w(5, 5);

    for (int i = 0; i < 40; ++i)
        w.integrate(world, rng, 16);

    EXPECT_EQ(w.tileX(), 6); // turned Right per the script
    EXPECT_EQ(w.tileY(), 5);
    EXPECT_EQ(rng.calls(), 1);
}
