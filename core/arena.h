
#pragma once

#include <cstdint>

#include "grid.h"

namespace pyrelite {

// Generate a classic Bomberman-style arena deterministically from a seed:
// an indestructible border, a pillar lattice on even/even cells, destructible
// bricks scattered across the rest, and the top-left spawn kept clear so the
// player can always move out. Same seed -> same arena.
Grid generateArena(int width, int height, std::uint64_t seed);

} // namespace pyrelite
