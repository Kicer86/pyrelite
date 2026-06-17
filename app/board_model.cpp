
#include "board_model.h"

#include <algorithm>
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

int BoardModel::explosionCount() const
{
    return static_cast<int>(m_game.explosions().size());
}

int BoardModel::powerUpCount() const
{
    return static_cast<int>(m_game.powerUps().size());
}

int BoardModel::playerMoveMs() const
{
    return std::max(30, 80 - (m_game.playerSpeed() - 1) * 10);
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

int BoardModel::explosionX(int index) const
{
    return m_game.explosions().at(index).x;
}

int BoardModel::explosionY(int index) const
{
    return m_game.explosions().at(index).y;
}

int BoardModel::powerUpX(int index) const
{
    return m_game.powerUps().at(index).x;
}

int BoardModel::powerUpY(int index) const
{
    return m_game.powerUps().at(index).y;
}

int BoardModel::powerUpType(int index) const
{
    switch (m_game.powerUps().at(index).type)
    {
    case pyrelite::PowerUpType::BombLimit:
        return BombLimitPowerUp;
    case pyrelite::PowerUpType::BombRange:
        return BombRangePowerUp;
    case pyrelite::PowerUpType::Speed:
        break;
    }
    return SpeedPowerUp;
}

void BoardModel::emitChanged()
{
    ++m_revision;
    emit changed();
}

void BoardModel::apply(pyrelite::Direction dir)
{
    if (m_game.tryMove(dir))
        emitChanged();
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
        emitChanged();
}

void BoardModel::update(int deltaMs)
{
    if (deltaMs <= 0)
        return;

    m_accumulator += deltaMs;
    bool changed = false;
    while (m_accumulator >= kStepMs)
    {
        m_accumulator -= kStepMs;
        if (m_game.update(kStepMs))
            changed = true;
    }
    if (changed)
        emitChanged();
}
