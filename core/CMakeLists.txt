
add_library(core STATIC
    grid/grid.cpp
    rng/rng.cpp
    terrain/arena.cpp
    world/world_gen.cpp
    world/world.cpp
    game/game.cpp
    enemies/enemy.cpp
    enemies/wanderer.cpp
    enemies/chaser.cpp
    enemies/bouncer.cpp
    enemies/hunter.cpp
    game/fixed_timestep.cpp
)

target_include_directories(core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
