
#include "board_model.h"

#include <optional>
#include <cstdint>

#include "version.h"
#include "world/chunk.h"
#include "world/world_gen.h"

namespace
{
    // The game runs on the infinite streamed World (see core/world.h): the view
    // renders a culled window of chunks around the player (Main.qml camera), so there
    // is no fixed board size — only a seed.
    constexpr std::uint64_t kSeed = 1; // fixed for now; run seeds come later
    constexpr int kStepMs = 16;        // ~60 Hz simulation quantum
    constexpr double kMaxFrameMs = 250; // cap catch-up after the render loop stalls

    // Presentation only: the enemy archetype -> how it looks. This is the single
    // place enemy art lives, kept out of the headless core and out of the QML. Today
    // it is a placeholder fill colour; when the discs become sprites this returns an
    // asset/animation source instead, and the generic delegate follows — no other
    // view change, and adding an archetype adds one case here.
    QString enemyColorFor(pyrelite::EnemyType type)
    {
        switch (type)
        {
        case pyrelite::EnemyType::Chaser:
            return QStringLiteral("#7d3cff"); // greedy chaser — menacing violet
        case pyrelite::EnemyType::Bouncer:
            return QStringLiteral("#17c0eb"); // ricochet — electric cyan
        case pyrelite::EnemyType::Hunter:
            return QStringLiteral("#ff1493"); // pathfinder — aggressive deep pink
        case pyrelite::EnemyType::Ghost:
            return QStringLiteral("#b3eaf6ff"); // wall-passer — translucent spectral
        case pyrelite::EnemyType::Wanderer:
            break;
        }
        return QStringLiteral("#d23b3b"); // roamer — red
    }

    // Presentation only: a perk's player-facing label + crystal colour. Like
    // enemyColorFor, this is the single place perk art lives, kept out of the headless
    // core; adding a perk adds one case here. The palette is a warm "loot" family
    // (gold/amber/green), distinct from the cool brick power-up diamonds.
    struct PerkInfo
    {
        QString name;
        QString color;
    };

    PerkInfo perkInfoFor(pyrelite::PerkType perk)
    {
        switch (perk)
        {
        case pyrelite::PerkType::ExtraBomb:
            return {QStringLiteral("Extra Bomb"), QStringLiteral("#ffcf40")};
        case pyrelite::PerkType::BiggerBlast:
            return {QStringLiteral("Bigger Blast"), QStringLiteral("#ff8c42")};
        case pyrelite::PerkType::SwiftFeet:
            break;
        }
        return {QStringLiteral("Swift Feet"), QStringLiteral("#7ee0a0")};
    }

    pyrelite::Direction toCore(BoardModel::Direction dir)
    {
        switch (dir)
        {
        case BoardModel::Up:
            return pyrelite::Direction::Up;
        case BoardModel::Down:
            return pyrelite::Direction::Down;
        case BoardModel::Left:
            return pyrelite::Direction::Left;
        case BoardModel::Right:
            break;
        }
        return pyrelite::Direction::Right;
    }
}

BoardModel::BoardModel(QObject *parent)
    : QObject(parent)
    , m_game(kSeed, pyrelite::Game::Streamed{})
    , m_step(kStepMs, kMaxFrameMs)
{
}

qreal BoardModel::playerX() const
{
    return m_game.playerSubX() / static_cast<qreal>(pyrelite::kSubcell);
}

qreal BoardModel::playerY() const
{
    return m_game.playerSubY() / static_cast<qreal>(pyrelite::kSubcell);
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

int BoardModel::enemyCount() const
{
    return static_cast<int>(m_game.enemies().size());
}

int BoardModel::level() const
{
    return m_game.level();
}

int BoardModel::xp() const
{
    return m_game.xp();
}

int BoardModel::xpToNextLevel() const
{
    return m_game.xpToNextLevel();
}

int BoardModel::perkCrystalCount() const
{
    return static_cast<int>(m_game.perkCrystals().size());
}

BoardModel::State BoardModel::state() const
{
    switch (m_game.state())
    {
    case pyrelite::GameState::Won:
        return Won;
    case pyrelite::GameState::Lost:
        return Lost;
    case pyrelite::GameState::Playing:
        break;
    }
    return Playing;
}

QString BoardModel::version() const
{
    return QString::fromLatin1(pyrelite::kGitSha) + QStringLiteral(" · ")
         + QString::fromLatin1(pyrelite::kBuildDate);
}

int BoardModel::tileAt(int x, int y) const
{
    switch (m_game.tileAt(x, y))
    {
    case pyrelite::Tile::Wall:
        return Wall;
    case pyrelite::Tile::Brick:
        return Brick;
    case pyrelite::Tile::Void:
        return Void;
    case pyrelite::Tile::Empty:
        break;
    }
    return Empty;
}

int BoardModel::tierAt(int x, int y) const
{
    return pyrelite::worldTier(pyrelite::chunkOf(x), pyrelite::chunkOf(y));
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

qreal BoardModel::enemyX(int index) const
{
    return m_game.enemies().at(index)->subX() / static_cast<qreal>(pyrelite::kSubcell);
}

qreal BoardModel::enemyY(int index) const
{
    return m_game.enemies().at(index)->subY() / static_cast<qreal>(pyrelite::kSubcell);
}

QString BoardModel::enemyColor(int index) const
{
    return enemyColorFor(m_game.enemies().at(index)->type());
}

int BoardModel::perkCrystalX(int index) const
{
    return m_game.perkCrystals().at(index).x;
}

int BoardModel::perkCrystalY(int index) const
{
    return m_game.perkCrystals().at(index).y;
}

QString BoardModel::perkCrystalColor(int index) const
{
    return perkInfoFor(m_game.perkCrystals().at(index).type).color;
}

QString BoardModel::perkCrystalName(int index) const
{
    return perkInfoFor(m_game.perkCrystals().at(index).type).name;
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

void BoardModel::setDirection(Direction dir)
{
    m_activeDir = dir;
    m_game.setMoveDirection(toCore(dir));
}

void BoardModel::clearDirection(Direction dir)
{
    if (m_activeDir != dir)
        return; // a different key is held; keep moving
    m_activeDir = -1;
    m_game.setMoveDirection(std::nullopt);
}

void BoardModel::placeBomb()
{
    m_game.queueBomb();
}

void BoardModel::setVisibleArea(int minX, int minY, int maxX, int maxY)
{
    m_game.setVisibleArea(minX, minY, maxX, maxY);
}

void BoardModel::update(double deltaMs)
{
    if (m_game.state() != pyrelite::GameState::Playing)
        return; // run ended; nothing ticks until restart()

    const int steps = m_step.advance(deltaMs);
    bool changed = false;
    for (int i = 0; i < steps; ++i)
    {
        if (m_game.update(m_step.stepMs()))
            changed = true;
    }
    if (changed)
        emitChanged();
}

void BoardModel::restart()
{
    m_game = pyrelite::Game(kSeed, pyrelite::Game::Streamed{});
    m_step = pyrelite::FixedTimestep(kStepMs, kMaxFrameMs);
    m_activeDir = -1;
    emitChanged();
}
