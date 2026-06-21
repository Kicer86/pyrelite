
#pragma once

#include <map>
#include <optional>
#include <utility>

#include "grid/grid.h"

namespace pyrelite
{
    // A sparse overlay of the player's irreversible changes to world terrain, keyed
    // by GLOBAL tile coordinate. The world is regenerated deterministically from its
    // seed; these deltas are reapplied on top whenever a chunk (re)materializes, so a
    // bombed-open area stays open on revisit — the player cannot re-farm it. Memory is
    // proportional to what the player has actually disturbed, not to world size.
    class DeltaStore
    {
    public:
        void record(int globalX, int globalY, Tile tile)
        {
            m_changes[{globalX, globalY}] = tile;
        }

        // The override at a global tile, or nullopt if the player never changed it
        // (then the deterministic base from the generator stands).
        std::optional<Tile> at(int globalX, int globalY) const
        {
            const auto it = m_changes.find({globalX, globalY});
            if (it == m_changes.end())
                return std::nullopt;
            return it->second;
        }

        std::size_t size() const { return m_changes.size(); }

    private:
        std::map<std::pair<int, int>, Tile> m_changes;
    };
} // namespace pyrelite
