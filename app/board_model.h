
#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

#include "fixed_timestep.h"
#include "game.h"

// QML-facing adapter over the core Game: board size + tiles, the player, bombs,
// and explosion flames. A single changed() signal (plus a bumping revision used
// to re-read tiles) refreshes the view after any state change. The player position
// is reported in fractional tile units so the view renders continuous movement.
class BoardModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int columns READ columns CONSTANT)
    Q_PROPERTY(int rows READ rows CONSTANT)
    Q_PROPERTY(qreal playerX READ playerX NOTIFY changed)
    Q_PROPERTY(qreal playerY READ playerY NOTIFY changed)
    Q_PROPERTY(int bombCount READ bombCount NOTIFY changed)
    Q_PROPERTY(int explosionCount READ explosionCount NOTIFY changed)
    Q_PROPERTY(int powerUpCount READ powerUpCount NOTIFY changed)
    Q_PROPERTY(int revision READ revision NOTIFY changed)

public:
    enum Tile { Empty, Wall, Brick };
    Q_ENUM(Tile)
    enum PowerUp { BombLimitPowerUp, BombRangePowerUp, SpeedPowerUp };
    Q_ENUM(PowerUp)
    enum Direction { Up, Down, Left, Right };
    Q_ENUM(Direction)

    explicit BoardModel(QObject *parent = nullptr);

    int columns() const;
    int rows() const;
    qreal playerX() const;
    qreal playerY() const;
    int bombCount() const;
    int explosionCount() const;
    int powerUpCount() const;
    int revision() const { return m_revision; }

    Q_INVOKABLE int tileAt(int x, int y) const;
    Q_INVOKABLE int bombX(int index) const;
    Q_INVOKABLE int bombY(int index) const;
    Q_INVOKABLE int explosionX(int index) const;
    Q_INVOKABLE int explosionY(int index) const;
    Q_INVOKABLE int powerUpX(int index) const;
    Q_INVOKABLE int powerUpY(int index) const;
    Q_INVOKABLE int powerUpType(int index) const;

    // Held-key movement: a press sets the direction, a release clears it only if it
    // is still the active one (last press wins). The core moves the player on its tick.
    Q_INVOKABLE void setDirection(Direction dir);
    Q_INVOKABLE void clearDirection(Direction dir);
    Q_INVOKABLE void placeBomb();
    Q_INVOKABLE void update(double deltaMs);

signals:
    void changed();

private:
    void emitChanged();

    pyrelite::Game m_game;
    pyrelite::FixedTimestep m_step;
    int m_revision = 0;
    int m_activeDir = -1;
};
