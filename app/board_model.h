
#pragma once

#include <map>
#include <optional>
#include <utility>
#include <vector>

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include "game/fixed_timestep.h"
#include "game/game.h"
#include "world/chunk.h"
#include "world/zone_cache.h"

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
    Q_PROPERTY(int score READ score NOTIFY changed)
    Q_PROPERTY(int maxDepth READ maxDepth NOTIFY changed)
    Q_PROPERTY(int perkCrystalCount READ perkCrystalCount NOTIFY changed)
    // Upgrade state for the HUD: the numeric power-up economy and the active perk
    // abilities. All refresh on the same changed() signal as the rest of the model.
    Q_PROPERTY(int bombLimit READ bombLimit NOTIFY changed)
    Q_PROPERTY(int bombRange READ bombRange NOTIFY changed)
    Q_PROPERTY(int playerSpeed READ playerSpeed NOTIFY changed)
    Q_PROPERTY(bool pierceBlast READ pierceBlast NOTIFY changed)
    Q_PROPERTY(int shieldCharges READ shieldCharges NOTIFY changed)
    Q_PROPERTY(bool remoteDetonator READ remoteDetonator NOTIFY changed)
    Q_PROPERTY(int previewChunkCount READ previewChunkCount NOTIFY changed)
    Q_PROPERTY(int chunkTiles READ chunkTiles CONSTANT)
    Q_PROPERTY(int revision READ revision NOTIFY changed)
    Q_PROPERTY(State state READ state NOTIFY changed)
    Q_PROPERTY(QString version READ version CONSTANT)

public:
    enum Tile { Empty, Wall, Brick, Void, Unknown };
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
    // Run score and furthest depth reached, read straight from the core for the HUD.
    int score() const { return m_game.score(); }
    int maxDepth() const { return m_game.maxDepth(); }
    int perkCrystalCount() const;
    // Upgrade state, read straight from the core for the HUD.
    int bombLimit() const { return m_game.bombLimit(); }
    int bombRange() const { return m_game.bombRange(); }
    int playerSpeed() const { return m_game.playerSpeed(); }
    bool pierceBlast() const { return m_game.pierceBlast(); }
    int shieldCharges() const { return m_game.shieldCharges(); }
    bool remoteDetonator() const { return m_game.remoteDetonator(); }
    int previewChunkCount() const { return static_cast<int>(m_previewChunks.size()); }
    // Tiles per world chunk, so the preview's chunk grid stays aligned with core
    // generation without the view hard-coding the size.
    int chunkTiles() const { return pyrelite::kChunkSize; }
    int revision() const { return m_revision; }
    State state() const;
    QString version() const;

    Q_INVOKABLE int tileAt(int x, int y) const;
    // Preview-only lookup: returns Unknown unless the containing chunk has already
    // entered the fixed generation window around the free camera. Never generates.
    Q_INVOKABLE int previewTileAt(int x, int y) const;
    Q_INVOKABLE bool previewChunkGenerated(int chunkX, int chunkY) const;
    // Permanently discover a fixed chunk window around the preview camera. Its radius
    // is independent of zoom, so zooming out exposes Unknown terrain at the edges.
    Q_INVOKABLE void generatePreviewAround(int centerX, int centerY);
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
    // Fraction of the flame's life left, 1 at ignition down to 0 as it dies. The
    // view uses it to burst the flame in and fade it out on the core's own clock.
    Q_INVOKABLE qreal explosionLife(int index) const;
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

    // Held-key movement: a press pushes a direction and a release removes it. The
    // newest still-held key drives the player, so releasing one key while another is
    // still down resumes that other key instead of stopping — the multi-key handling
    // a held diagonal needs on web, where the OS does not re-issue the held key.
    Q_INVOKABLE void setDirection(Direction dir);
    Q_INVOKABLE void clearDirection(Direction dir);
    // Drop every held direction and stop. Called when the window loses focus so a key
    // whose release never arrives cannot leave the player walking forever.
    Q_INVOKABLE void clearDirections();
    Q_INVOKABLE void placeBomb();
    // Trigger the Remote Detonator perk: blow every live bomb. A no-op without the perk.
    Q_INVOKABLE void detonateBombs();
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
    // Directions whose key is currently held, in press order; the back is the active
    // one. Releasing a key removes it and falls back to the previous still-held key.
    std::vector<Direction> m_heldDirs;
    std::map<std::pair<int, int>, pyrelite::Chunk> m_previewChunks;
    std::optional<std::pair<int, int>> m_previewCenterChunk;
    // Builds each preview zone once instead of rebuilding it for every chunk it owns.
    pyrelite::ZoneCache m_previewZones;
    // One-entry memo for previewTileAt (see board_model.cpp). std::map keeps element
    // pointers stable across inserts, and generatePreviewAround clears this on growth.
    mutable std::optional<std::pair<int, int>> m_previewLookupKey;
    mutable const pyrelite::Chunk *m_previewLookupChunk = nullptr;
};
