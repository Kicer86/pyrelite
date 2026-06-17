
#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

#include "game.h"

// QML-facing adapter over the core Game: board size, per-cell tiles, the player
// position + movement, and active bombs.
class BoardModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int columns READ columns CONSTANT)
    Q_PROPERTY(int rows READ rows CONSTANT)
    Q_PROPERTY(int playerX READ playerX NOTIFY playerMoved)
    Q_PROPERTY(int playerY READ playerY NOTIFY playerMoved)
    Q_PROPERTY(int bombCount READ bombCount NOTIFY bombsChanged)

public:
    enum Tile { Empty, Wall, Brick };
    Q_ENUM(Tile)

    explicit BoardModel(QObject *parent = nullptr);

    int columns() const;
    int rows() const;
    int playerX() const;
    int playerY() const;
    int bombCount() const;

    Q_INVOKABLE int tileAt(int x, int y) const;
    Q_INVOKABLE int bombX(int index) const;
    Q_INVOKABLE int bombY(int index) const;

    Q_INVOKABLE void moveUp();
    Q_INVOKABLE void moveDown();
    Q_INVOKABLE void moveLeft();
    Q_INVOKABLE void moveRight();
    Q_INVOKABLE void placeBomb();
    Q_INVOKABLE void update(int deltaMs);

signals:
    void playerMoved();
    void bombsChanged();

private:
    void apply(pyrelite::Direction dir);

    pyrelite::Game m_game;
};
