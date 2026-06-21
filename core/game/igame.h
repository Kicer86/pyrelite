
#pragma once

namespace pyrelite
{
    // The slice of the world an entity sees while deciding how to move: tile
    // walkability and the player's position. Game implements it; enemy AI depends on
    // this interface (not the concrete Game), so unit tests can drive behaviour with
    // a fake arena. Grows as new archetypes need more queries.
    class IGame
    {
    public:
        virtual ~IGame() = default;

        // Whether a moving entity may occupy tile (x, y): in-bounds, empty, and not
        // blocked by a live bomb.
        virtual bool walkable(int x, int y) const = 0;

        // As walkable, but bricks are passable too — only solid walls (and bombs) stop
        // the mover. The wall-passing Ghost navigates by this; everyone else uses
        // walkable.
        virtual bool walkableThroughBricks(int x, int y) const = 0;

        // The tile the player currently occupies.
        virtual int playerX() const = 0;
        virtual int playerY() const = 0;
    };
} // namespace pyrelite
