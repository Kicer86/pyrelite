
#include "game/zone_spawns.h"

#include <cstdint>
#include <cstdlib>

#include <gtest/gtest.h>

#include "world/chunk.h"
#include "world/world_gen.h"
#include "world/zone.h"

using namespace pyrelite;

TEST(ZoneSpawnsTest, RosterIsDeterministic)
{
    for (std::uint64_t seed = 1; seed <= 4; ++seed)
        for (int zone : {0, 1, 3, -2})
        {
            const auto a = zoneEnemyRoster(seed, zone, zone);
            const auto b = zoneEnemyRoster(seed, zone, zone);
            ASSERT_EQ(a.size(), b.size()) << "seed " << seed << " zone " << zone;
            for (std::size_t i = 0; i < a.size(); ++i)
            {
                EXPECT_EQ(a[i].x, b[i].x);
                EXPECT_EQ(a[i].y, b[i].y);
                EXPECT_EQ(a[i].type, b[i].type);
            }
        }
}

TEST(ZoneSpawnsTest, CountRisesWithDistanceThenSaturates)
{
    EXPECT_LT(zoneEnemyCount(0), zoneEnemyCount(2));
    EXPECT_LT(zoneEnemyCount(2), zoneEnemyCount(4));
    // The origin zone is the calmest; a far zone seeds more enemies (until the tier caps).
    for (std::uint64_t seed = 1; seed <= 8; ++seed)
    {
        const auto origin = zoneEnemyRoster(seed, 0, 0);
        const auto far = zoneEnemyRoster(seed, 5, 0);
        EXPECT_LT(origin.size(), far.size()) << "seed " << seed;
    }
}

TEST(ZoneSpawnsTest, TougherArchetypesAreGatedToHigherTiers)
{
    bool tier0Hunter = false;
    for (int slot = 0; slot < zoneEnemyCount(0); ++slot)
        tier0Hunter |= zoneEnemyType(0, slot) == EnemyType::Hunter;
    EXPECT_FALSE(tier0Hunter); // the opening tier has no relentless Hunters

    bool farHunter = false;
    bool farGhost = false;
    for (int slot = 0; slot < zoneEnemyCount(4); ++slot)
    {
        const EnemyType type = zoneEnemyType(4, slot);
        farHunter |= type == EnemyType::Hunter;
        farGhost |= type == EnemyType::Ghost;
    }
    EXPECT_TRUE(farHunter);
    EXPECT_TRUE(farGhost);
}

TEST(ZoneSpawnsTest, OriginRosterKeepsClearOfTheSpawnPocket)
{
    for (std::uint64_t seed = 1; seed <= 8; ++seed)
        for (const EnemySpawn &enemy : zoneEnemyRoster(seed, 0, 0))
            EXPECT_GE(std::abs(enemy.x - 1) + std::abs(enemy.y - 1), 4) << "seed " << seed;
}

TEST(ZoneSpawnsTest, SpawnsSitOnGeneratedFloor)
{
    for (std::uint64_t seed = 1; seed <= 4; ++seed)
        for (int zx : {0, 2, -1})
            for (int zy : {0, 1})
            {
                const Zone zone = generateZone(seed, zx, zy);
                const int baseX = Zone::minChunk(zx) * kChunkSize;
                const int baseY = Zone::minChunk(zy) * kChunkSize;
                for (const EnemySpawn &enemy : zoneEnemyRoster(seed, zx, zy))
                    EXPECT_EQ(zone.at(enemy.x - baseX, enemy.y - baseY), Tile::Empty)
                        << "seed " << seed << " zone " << zx << "," << zy;
            }
}
