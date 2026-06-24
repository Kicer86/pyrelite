
#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include "game/fixed_timestep.h"
#include "game/game.h"

// QML-facing adapter over the core Game: board size + tiles, the player, bombs,
// and explosion flames. A single changed() signal (plus a bumping revision used
// to re-read tiles) refreshes the view after any state change. The player position
// is reported in fractional tile units so the view renders continuous movement.
class BoardModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(qreal playerX READ playerX NOTIFY changed)
    Q_PROPERTY(qreal playerY READ playerY NOTIFY changed)
    Q_PROPERTY(int bombCount READ bombCount NOTIFY changed)
    Q_PROPERTY(int explosionCount READ explosionCount NOTIFY changed)
    Q_PROPERTY(int powerUpCount READ powerUpCount NOTIFY changed)
    Q_PROPERTY(int enemyCount READ enemyCount NOTIFY changed)
    Q_PROPERTY(int level READ level NOTIFY changed)
    Q_PROPERTY(int xp READ xp NOTIFY changed)
    Q_PROPERTY(int xpToNextLevel READ xpToNextLevel NOTIFY changed)
    Q_PROPERTY(int perkCrystalCount READ perkCrystalCount NOTIFY changed)
    Q_PROPERTY(int revision READ revision NOTIFY changed)
    Q_PROPERTY(State state READ state NOTIFY changed)
    Q_PROPERTY(QString version READ version CONSTANT)

public:
    enum Tile { Empty, Wall, Brick, Void };
    Q_ENUM(Tile)
    enum PowerUp { BombLimitPowerUp, BombRangePowerUp, SpeedPowerUp };
    Q_ENUM(PowerUp)
    enum Direction { Up, Down, Left, Right };
    Q_ENUM(Direction)
    enum State { Playing, Won, Lost };
    Q_ENUM(State)

    explicit BoardModel(QObject *parent = nullptr);

    qreal playerX() const;
    qreal playerY() const;
    int bombCount() const;
    int explosionCount() const;
    int powerUpCount() const;
    int enemyCount() const;
    int level() const;
    int xp() const;
    int xpToNextLevel() const;
    int perkCrystalCount() const;
    int revision() const { return m_revision; }
    State state() const;
    QString version() const;

    Q_INVOKABLE int tileAt(int x, int y) const;
    // The world tier (difficulty/theme band) of a global tile, rising with distance
    // from spawn. The view maps it to a region palette; the policy lives once in core.
    Q_INVOKABLE int tierAt(int x, int y) const;
    Q_INVOKABLE int bombX(int index) const;
    Q_INVOKABLE int bombY(int index) const;
    // Fraction of the fuse left, 1 just-placed down to 0 at detonation. The view
    // uses it to tighten the bomb's pulse and redden it as the blast nears.
    Q_INVOKABLE qreal bombFuse(int index) const;
    Q_INVOKABLE int explosionX(int index) const;
    Q_INVOKABLE int explosionY(int index) const;
    Q_INVOKABLE int powerUpX(int index) const;
    Q_INVOKABLE int powerUpY(int index) const;
    Q_INVOKABLE int powerUpType(int index) const;
    // Enemy positions in fractional tile units, so continuous movement renders
    // straight from the core (like the player) with no view-side easing.
    Q_INVOKABLE qreal enemyX(int index) const;
    Q_INVOKABLE qreal enemyY(int index) const;
    // How to draw the enemy at index. The view renders whatever these return, so it
    // carries no per-archetype logic; this is the single seam where enemy art lives.
    // enemyColor is the body tint; enemyKind names the silhouette the sprite picks.
    Q_INVOKABLE QString enemyColor(int index) const;
    Q_INVOKABLE QString enemyKind(int index) const;
    // A floor perk crystal's tile position, fill colour and label. Like enemyColor,
    // this is the single app-side seam where perk presentation lives; the core only
    // names the type.
    Q_INVOKABLE int perkCrystalX(int index) const;
    Q_INVOKABLE int perkCrystalY(int index) const;
    Q_INVOKABLE QString perkCrystalColor(int index) const;
    Q_INVOKABLE QString perkCrystalName(int index) const;

    // Held-key movement: a press sets the direction, a release clears it only if it
    // is still the active one (last press wins). The core moves the player on its tick.
    Q_INVOKABLE void setDirection(Direction dir);
    Q_INVOKABLE void clearDirection(Direction dir);
    Q_INVOKABLE void placeBomb();
    Q_INVOKABLE void setVisibleArea(int minX, int minY, int maxX, int maxY);
    Q_INVOKABLE void update(double deltaMs);
    // Start a fresh run (same seed) after a loss.
    Q_INVOKABLE void restart();

signals:
    void changed();

private:
    void emitChanged();

    pyrelite::Game m_game;
    pyrelite::FixedTimestep m_step;
    int m_revision = 0;
    int m_activeDir = -1;
};
