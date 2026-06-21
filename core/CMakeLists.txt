
add_library(core STATIC
    grid.cpp
    rng.cpp
    arena.cpp
    world_gen.cpp
    world.cpp
    game.cpp
    enemy.cpp
    wanderer.cpp
    chaser.cpp
    bouncer.cpp
    hunter.cpp
    fixed_timestep.cpp
)

target_include_directories(core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
