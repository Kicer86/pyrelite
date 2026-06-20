
#include "board_model.h"

#include <optional>
#include <cstdint>

#include "version.h"

namespace
{
    constexpr int kColumns = 13;
    constexpr int kRows = 11;
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

    // Presentation only: a perk's player-facing label + one-line blurb. Like
    // enemyColorFor, this is the single place perk copy lives, kept out of the headless
    // core; adding a perk adds one case here.
    struct PerkInfo
    {
        QString name;
        QString description;
    };

    PerkInfo perkInfoFor(pyrelite::PerkType perk)
    {
        switch (perk)
        {
        case pyrelite::PerkType::ExtraBomb:
            return {QStringLiteral("Extra Bomb"),
                    QStringLiteral("Carry one more bomb at a time.")};
        case pyrelite::PerkType::BiggerBlast:
            return {QStringLiteral("Bigger Blast"),
                    QStringLiteral("Your explosions reach one tile further.")};
        case pyrelite::PerkType::SwiftFeet:
            break;
        }
        return {QStringLiteral("Swift Feet"),
                QStringLiteral("Move a little faster.")};
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
    , m_game(kColumns, kRows, kSeed)
    , m_step(kStepMs, kMaxFrameMs)
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

int BoardModel::perkChoiceCount() const
{
    return static_cast<int>(m_game.perkChoices().size());
}

BoardModel::State BoardModel::state() const
{
    switch (m_game.state())
    {
    case pyrelite::GameState::Won:
        return Won;
    case pyrelite::GameState::Lost:
        return Lost;
    case pyrelite::GameState::LevelUp:
        return LevelUp;
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

QString BoardModel::perkName(int index) const
{
    return perkInfoFor(m_game.perkChoices().at(index)).name;
}

QString BoardModel::perkDescription(int index) const
{
    return perkInfoFor(m_game.perkChoices().at(index)).description;
}

void BoardModel::choosePerk(int index)
{
    if (m_game.choosePerk(index))
        emitChanged();
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
    m_game = pyrelite::Game(kColumns, kRows, kSeed);
    m_step = pyrelite::FixedTimestep(kStepMs, kMaxFrameMs);
    m_activeDir = -1;
    emitChanged();
}
