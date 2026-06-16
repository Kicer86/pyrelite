
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

## Tests

```
ctest --test-dir build
```
