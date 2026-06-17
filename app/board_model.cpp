
#include "board_model.h"

#include <cstdint>

namespace {
constexpr int kColumns = 13;
constexpr int kRows = 11;
constexpr std::uint64_t kSeed = 1; // fixed for now; run seeds come later
} // namespace

BoardModel::BoardModel(QObject *parent)
    : QObject(parent)
    , game_(kColumns, kRows, kSeed)
{
}

int BoardModel::columns() const
{
    return game_.grid().width();
}

int BoardModel::rows() const
{
    return game_.grid().height();
}

int BoardModel::playerX() const
{
    return game_.playerX();
}

int BoardModel::playerY() const
{
    return game_.playerY();
}

int BoardModel::tileAt(int x, int y) const
{
    switch (game_.grid().at(x, y)) {
    case pyrelite::Tile::Wall:
        return Wall;
    case pyrelite::Tile::Brick:
        return Brick;
    case pyrelite::Tile::Empty:
        break;
    }
    return Empty;
}

void BoardModel::apply(pyrelite::Direction dir)
{
    if (game_.tryMove(dir))
        emit playerMoved();
}

void BoardModel::moveUp()
{
    apply(pyrelite::Direction::Up);
}

void BoardModel::moveDown()
{
    apply(pyrelite::Direction::Down);
}

void BoardModel::moveLeft()
{
    apply(pyrelite::Direction::Left);
}

void BoardModel::moveRight()
{
    apply(pyrelite::Direction::Right);
}
