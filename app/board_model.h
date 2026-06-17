
#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

#include "game.h"

// QML-facing adapter over the core Game: board size + tiles, the player, bombs,
// and explosion flames. A single changed() signal (plus a bumping revision used
// to re-read tiles) refreshes the view after any state change.
class BoardModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int columns READ columns CONSTANT)
    Q_PROPERTY(int rows READ rows CONSTANT)
    Q_PROPERTY(int playerX READ playerX NOTIFY changed)
    Q_PROPERTY(int playerY READ playerY NOTIFY changed)
    Q_PROPERTY(int bombCount READ bombCount NOTIFY changed)
    Q_PROPERTY(int explosionCount READ explosionCount NOTIFY changed)
    Q_PROPERTY(int powerUpCount READ powerUpCount NOTIFY changed)
    Q_PROPERTY(int playerMoveMs READ playerMoveMs NOTIFY changed)
    Q_PROPERTY(int revision READ revision NOTIFY changed)

public:
    enum Tile { Empty, Wall, Brick };
    Q_ENUM(Tile)
    enum PowerUp { BombLimitPowerUp, BombRangePowerUp, SpeedPowerUp };
    Q_ENUM(PowerUp)

    explicit BoardModel(QObject *parent = nullptr);

    int columns() const;
    int rows() const;
    int playerX() const;
    int playerY() const;
    int bombCount() const;
    int explosionCount() const;
    int powerUpCount() const;
    int playerMoveMs() const;
    int revision() const { return m_revision; }

    Q_INVOKABLE int tileAt(int x, int y) const;
    Q_INVOKABLE int bombX(int index) const;
    Q_INVOKABLE int bombY(int index) const;
    Q_INVOKABLE int explosionX(int index) const;
    Q_INVOKABLE int explosionY(int index) const;
    Q_INVOKABLE int powerUpX(int index) const;
    Q_INVOKABLE int powerUpY(int index) const;
    Q_INVOKABLE int powerUpType(int index) const;

    Q_INVOKABLE void moveUp();
    Q_INVOKABLE void moveDown();
    Q_INVOKABLE void moveLeft();
    Q_INVOKABLE void moveRight();
    Q_INVOKABLE void placeBomb();
    Q_INVOKABLE void update(int deltaMs);

signals:
    void changed();

private:
    void apply(pyrelite::Direction dir);
    void emitChanged();

    pyrelite::Game m_game;
    int m_revision = 0;
};
