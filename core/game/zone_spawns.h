
#pragma once

#include <cstdint>
#include <vector>

#include "enemies/ienemy.h"

namespace pyrelite
{
    // One enemy the world seeds when a generation ZONE activates: where (GLOBAL tile) and
    // which archetype. Spawn cells sit on pristine generated floor — never a destructible
    // the player could clear — so a zone's roster is a pure function of (seed, zone) and
    // never drifts as the player edits terrain. Persisting a kill therefore needs only the
    // cell, and a reactivated zone reproduces exactly the same roster minus its dead.
    struct EnemySpawn
    {
        int x;
        int y;
        EnemyType type;
    };

    // The deterministic enemy roster of the zone at (zoneX, zoneY). Count and archetype
    // mix scale with the zone's tier (distance from origin): the farther out, the more
    // enemies and the harder the mix (more Hunters/Ghosts). The origin zone keeps its
    // enemies a safe distance from the spawn pocket so the opening is never a death trap.
    // Pure: identical (seed, zoneX, zoneY) always yields the identical roster.
    std::vector<EnemySpawn> zoneEnemyRoster(std::uint64_t seed, int zoneX, int zoneY);

    // How many enemies a zone of the given tier seeds, and the archetype of its slot-th
    // enemy. Exposed for tests and reuse; the mix front-loads the tier's tougher
    // archetypes (Hunters, then Ghosts) so the escalation with distance is legible.
    int zoneEnemyCount(int tier);
    EnemyType zoneEnemyType(int tier, int slot);
} // namespace pyrelite
