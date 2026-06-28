
find_package(GTest REQUIRED)

include(GoogleTest)

set(tests
    grid/grid_test
    grid/movement_test
    rng/rng_test
    terrain/arena_test
    world/world_test
    world/world_gen_test
    world/streamed_game_test
    enemies/enemy_test
    enemies/enemy_ai_test
    game/game_test
    game/zone_spawns_test
    game/bomb_test
    game/explosion_test
    game/power_up_test
    game/death_test
    game/progression_test
    game/score_test
    game/perk_ability_test
    game/input_validation_test
    game/fixed_timestep_test
)

foreach(test ${tests})
    get_filename_component(name ${test} NAME)
    add_executable(${name} ${test}.cpp)
    target_link_libraries(${name} PRIVATE core GTest::gmock_main)
    gtest_discover_tests(${name})
endforeach()
