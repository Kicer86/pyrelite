
#include "game.h"

#include <utility>

#include "arena.h"

namespace pyrelite {

Game::Game(Grid grid)
    : grid_(std::move(grid))
    , playerX_(1)
    , playerY_(1)
{
}

Game::Game(int width, int height, std::uint64_t seed)
    : Game(generateArena(width, height, seed))
{
}

bool Game::walkable(int x, int y) const
{
    return grid_.inBounds(x, y) && grid_.at(x, y) == Tile::Empty;
}

bool Game::tryMove(Direction dir)
{
    int nx = playerX_;
    int ny = playerY_;
    switch (dir) {
    case Direction::Up:
        --ny;
        break;
    case Direction::Down:
        ++ny;
        break;
    case Direction::Left:
        --nx;
        break;
    case Direction::Right:
        ++nx;
        break;
    }

    if (!walkable(nx, ny))
        return false;

    playerX_ = nx;
    playerY_ = ny;
    return true;
}

} // namespace pyrelite
