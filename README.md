
# Pyrelite

A Bomberman-inspired roguelite: place bombs, destroy walls, grab power-ups, and grow
your character across runs.

- **Core gameplay logic** — pure C++ (`core/`), unit-tested, no UI dependencies.
- **View / UI** — Qt Quick / QML (`app/`).
- **Targets** — web (Qt for WebAssembly) and desktop (Steam).

## Build

```
cmake -B build
cmake --build build
```

## World preview

Run the desktop app with a free-flying terrain camera:

```
./build/app/pyrelite --preview
```

Use WASD or the arrow keys to fly, `+`/`-` to zoom, and `R` to return to the spawn.
Gameplay is paused while previewing. Zooming out expands the visible tile window, so
additional terrain is generated and streamed instead of scaling a fixed snapshot.

## Tests

```
ctest --test-dir build
```
