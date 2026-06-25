
#include "game/zone_spawns.h"

#include <cstdlib>
#include <utility>

#include "rng/rng.h"
#include "world/world_gen.h"
#include "world/zone.h"

namespace pyrelite
{
    namespace
    {
        // The base population of a tier-0 zone and the extra enemies each tier adds.
        // Tuned for the streamed run; raise/lower to make the world denser or sparser.
        constexpr int kZoneBaseEnemies = 3;
        constexpr int kZonePerTierEnemies = 2;

        // Origin enemies stay at least this far (Manhattan) from the spawn tile (1, 1),
        // so the opening pocket is never an immediate death trap.
        constexpr int kOriginSpawnClearance = 4;

        // A roster RNG seed independent of any live simulation stream, so a zone's roster
        // is the same however and whenever the zone is (re)activated. splitmix64 finaliser.
        std::uint64_t rosterSeed(std::uint64_t seed, int zoneX, int zoneY)
        {
            std::uint64_t h = seed ^ 0x5BD1E995A4C1F3B7ULL;
            h ^= static_cast<std::uint64_t>(static_cast<std::uint32_t>(zoneX))
                * 0xD1B54A32D192ED03ULL;
            h ^= static_cast<std::uint64_t>(static_cast<std::uint32_t>(zoneY))
                * 0x9E3779B97F4A7C15ULL;
            h = (h ^ (h >> 30)) * 0xBF58476D1CE4E5B9ULL;
            h = (h ^ (h >> 27)) * 0x94D049BB133111EBULL;
            return h ^ (h >> 31);
        }
    }

    int zoneEnemyCount(int tier)
    {
        return kZoneBaseEnemies + tier * kZonePerTierEnemies;
    }

    EnemyType zoneEnemyType(int tier, int slot)
    {
        // Reserve the first slots for the tier's tougher archetypes — more of them the
        // farther out — and let the remainder roam as Wanderers. The cutoffs are running
        // totals, so the order is Hunter, Ghost, Chaser, Bouncer, then Wanderer.
        const int hunters = tier;          // 0..kMaxTier
        const int ghosts = tier / 2;       // a slower ramp than Hunters
        const int chasers = 1 + tier / 2;  // always at least one greedy Chaser
        const int bouncers = 1;            // and one ricochet

        int cutoff = 0;
        if (slot < (cutoff += hunters))  return EnemyType::Hunter;
        if (slot < (cutoff += ghosts))   return EnemyType::Ghost;
        if (slot < (cutoff += chasers))  return EnemyType::Chaser;
        if (slot < (cutoff += bouncers)) return EnemyType::Bouncer;
        return EnemyType::Wanderer;
    }

    std::vector<EnemySpawn> zoneEnemyRoster(std::uint64_t seed, int zoneX, int zoneY)
    {
        // Build the PRISTINE zone (no player deltas) so spawn cells are stable floor and
        // the roster cannot be farmed by bombing open new ground.
        const Zone zone = generateZone(seed, zoneX, zoneY);
        const int tier = worldTier(Zone::minChunk(zoneX), Zone::minChunk(zoneY));
        const int count = zoneEnemyCount(tier);
        const int baseX = Zone::minChunk(zoneX) * kChunkSize;
        const int baseY = Zone::minChunk(zoneY) * kChunkSize;
        const bool isOrigin = zoneX == 0 && zoneY == 0;
        constexpr int kZoneLast = kZoneSize - 1;

        // Candidate floor cells (global tiles): Empty, with an Empty neighbour so the
        // enemy can roam, and clear of the spawn pocket in the origin zone. Gathered in
        // row-major order, then drawn with the roster RNG, so the set is reproducible.
        std::vector<std::pair<int, int>> candidates;
        for (int ly = 0; ly < kZoneSize; ++ly)
            for (int lx = 0; lx < kZoneSize; ++lx)
            {
                if (zone.at(lx, ly) != Tile::Empty)
                    continue;
                const int gx = baseX + lx;
                const int gy = baseY + ly;
                if (isOrigin
                    && std::abs(gx - 1) + std::abs(gy - 1) < kOriginSpawnClearance)
                    continue;
                const bool hasEmptyNeighbour =
                    (lx > 0 && zone.at(lx - 1, ly) == Tile::Empty)
                    || (lx < kZoneLast && zone.at(lx + 1, ly) == Tile::Empty)
                    || (ly > 0 && zone.at(lx, ly - 1) == Tile::Empty)
                    || (ly < kZoneLast && zone.at(lx, ly + 1) == Tile::Empty);
                if (!hasEmptyNeighbour)
                    continue;
                candidates.emplace_back(gx, gy);
            }

        Rng rng(rosterSeed(seed, zoneX, zoneY));
        std::vector<EnemySpawn> roster;
        for (int placed = 0; placed < count && !candidates.empty(); ++placed)
        {
            const std::size_t pick =
                rng.below(static_cast<std::uint32_t>(candidates.size()));
            const auto [gx, gy] = candidates[pick];
            candidates[pick] = candidates.back();
            candidates.pop_back();
            roster.push_back({gx, gy, zoneEnemyType(tier, placed)});
        }
        return roster;
    }
} // namespace pyrelite
