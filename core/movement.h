
#pragma once

namespace pyrelite
{
    enum class Direction { Up, Down, Left, Right };

    // Sub-cell resolution. Moving entities (the player and the enemies) store their
    // position in these integer sub-units, kSubcell per tile, so movement is
    // continuous yet bit-exact on every platform (no float). That keeps seeded runs
    // reproducible and headless asserts exact. Tile coords are pos / kSubcell.
    inline constexpr int kSubcell = 1024;

    // Step the tile coords (tx, ty) one cell along dir.
    inline void stepTile(Direction dir, int &tx, int &ty)
    {
        switch (dir)
        {
        case Direction::Up:
            --ty;
            break;
        case Direction::Down:
            ++ty;
            break;
        case Direction::Left:
            --tx;
            break;
        case Direction::Right:
            ++tx;
            break;
        }
    }

    // The opposite heading — used by enemies that ricochet straight back off a wall.
    inline Direction reverse(Direction dir)
    {
        switch (dir)
        {
        case Direction::Up:
            return Direction::Down;
        case Direction::Down:
            return Direction::Up;
        case Direction::Left:
            return Direction::Right;
        case Direction::Right:
            break;
        }
        return Direction::Left;
    }

    // Move cur toward target by at most maxStep, never overshooting.
    inline int approach(int cur, int target, int maxStep)
    {
        if (cur < target)
            return cur + maxStep < target ? cur + maxStep : target;
        if (cur > target)
            return cur - maxStep > target ? cur - maxStep : target;
        return cur;
    }

    // The tile a moving entity occupies: the centre nearest its sub-position.
    inline int tileOf(int sub)
    {
        return (sub + kSubcell / 2) / kSubcell;
    }
} // namespace pyrelite
