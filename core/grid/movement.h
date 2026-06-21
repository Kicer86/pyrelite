
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

    // The tile a moving entity occupies: the centre nearest its sub-position. Uses
    // floor division, so it stays correct for NEGATIVE positions — the streamed world
    // is global, and tiles west of / north of the origin have negative coordinates.
    // (Plain `/` truncates toward zero, which would put tile -1 and tile 0 both at 0,
    // making the collision tile disagree with the rendered tile: invisible walls.)
    inline int tileOf(int sub)
    {
        const int shifted = sub + kSubcell / 2;
        int tile = shifted / kSubcell;
        if (shifted % kSubcell != 0 && shifted < 0)
            --tile;
        return tile;
    }

    // The sub-cell bookkeeping every grid-locked mover shares (the player and the
    // enemies alike): a current sub-position and the tile centre it is travelling
    // toward. Each owner decides WHERE to head when centred and how fast to crawl;
    // this just holds the position, steps it, and reports the occupied tile — the
    // common half both movers used to re-implement.
    struct GridMover
    {
        int subX;
        int subY;
        int targetSubX;
        int targetSubY;

        GridMover(int tileX, int tileY)
            : subX(tileX * kSubcell)
            , subY(tileY * kSubcell)
            , targetSubX(tileX * kSubcell)
            , targetSubY(tileY * kSubcell)
        {
        }

        // Centred on a tile, i.e. no step in progress, so a new heading may be chosen.
        bool centred() const { return subX == targetSubX && subY == targetSubY; }

        int tileX() const { return tileOf(subX); }
        int tileY() const { return tileOf(subY); }

        // Aim the next step at the centre of tile (tx, ty).
        void aimAt(int tx, int ty)
        {
            targetSubX = tx * kSubcell;
            targetSubY = ty * kSubcell;
        }

        // Teleport onto tile (tx, ty), cancelling any step in progress.
        void snapTo(int tx, int ty)
        {
            subX = targetSubX = tx * kSubcell;
            subY = targetSubY = ty * kSubcell;
        }

        // Crawl toward the target by up to maxStep sub-units on each axis; returns
        // whether the position actually changed.
        bool advance(int maxStep)
        {
            const int beforeX = subX;
            const int beforeY = subY;
            subX = approach(subX, targetSubX, maxStep);
            subY = approach(subY, targetSubY, maxStep);
            return subX != beforeX || subY != beforeY;
        }
    };
} // namespace pyrelite
