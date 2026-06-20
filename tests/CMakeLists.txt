
find_package(GTest REQUIRED)

include(GoogleTest)

set(tests
    grid_test
    rng_test
    arena_test
    game_test
    bomb_test
    explosion_test
    power_up_test
    enemy_test
    enemy_ai_test
    death_test
    progression_test
    input_validation_test
    fixed_timestep_test
)

foreach(test ${tests})
    add_executable(${test} ${test}.cpp)
    target_link_libraries(${test} PRIVATE core GTest::gmock_main)
    gtest_discover_tests(${test})
endforeach()
