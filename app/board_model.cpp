
#include "board_model.h"

#include <cstdint>

#include "arena.h"

namespace {
constexpr int kColumns = 13;
constexpr int kRows = 11;
constexpr std::uint64_t kSeed = 1; // fixed for now; run seeds come later
} // namespace

BoardModel::BoardModel(QObject *parent)
    : QObject(parent)
    , grid_(pyrelite::generateArena(kColumns, kRows, kSeed))
{
}

int BoardModel::tileAt(int x, int y) const
{
    switch (grid_.at(x, y)) {
    case pyrelite::Tile::Wall:
        return Wall;
    case pyrelite::Tile::Brick:
        return Brick;
    case pyrelite::Tile::Empty:
        break;
    }
    return Empty;
}
