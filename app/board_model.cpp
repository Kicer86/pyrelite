
#include "board_model.h"

#include <cstdint>

namespace
{
    constexpr int kColumns = 13;
    constexpr int kRows = 11;
    constexpr std::uint64_t kSeed = 1; // fixed for now; run seeds come later
}

BoardModel::BoardModel(QObject *parent)
    : QObject(parent)
    , m_game(kColumns, kRows, kSeed)
{
}

int BoardModel::columns() const
{
    return m_game.grid().width();
}

int BoardModel::rows() const
{
    return m_game.grid().height();
}

int BoardModel::playerX() const
{
    return m_game.playerX();
}

int BoardModel::playerY() const
{
    return m_game.playerY();
}

int BoardModel::bombCount() const
{
    return static_cast<int>(m_game.bombs().size());
}

int BoardModel::tileAt(int x, int y) const
{
    switch (m_game.grid().at(x, y))
    {
    case pyrelite::Tile::Wall:
        return Wall;
    case pyrelite::Tile::Brick:
        return Brick;
    case pyrelite::Tile::Empty:
        break;
    }
    return Empty;
}

int BoardModel::bombX(int index) const
{
    return m_game.bombs().at(index).x;
}

int BoardModel::bombY(int index) const
{
    return m_game.bombs().at(index).y;
}

void BoardModel::apply(pyrelite::Direction dir)
{
    if (m_game.tryMove(dir))
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

void BoardModel::placeBomb()
{
    if (m_game.placeBomb())
        emit bombsChanged();
}

void BoardModel::update(int deltaMs)
{
    if (m_game.update(deltaMs))
        emit bombsChanged();
}
