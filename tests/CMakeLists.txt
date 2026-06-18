
find_package(GTest REQUIRED)

include(GoogleTest)

add_executable(grid_test grid_test.cpp)
target_link_libraries(grid_test PRIVATE core GTest::gmock_main)
gtest_discover_tests(grid_test)

add_executable(rng_test rng_test.cpp)
target_link_libraries(rng_test PRIVATE core GTest::gmock_main)
gtest_discover_tests(rng_test)

add_executable(arena_test arena_test.cpp)
target_link_libraries(arena_test PRIVATE core GTest::gmock_main)
gtest_discover_tests(arena_test)

add_executable(game_test game_test.cpp)
target_link_libraries(game_test PRIVATE core GTest::gmock_main)
gtest_discover_tests(game_test)

add_executable(bomb_test bomb_test.cpp)
target_link_libraries(bomb_test PRIVATE core GTest::gmock_main)
gtest_discover_tests(bomb_test)

add_executable(explosion_test explosion_test.cpp)
target_link_libraries(explosion_test PRIVATE core GTest::gmock_main)
gtest_discover_tests(explosion_test)

add_executable(power_up_test power_up_test.cpp)
target_link_libraries(power_up_test PRIVATE core GTest::gmock_main)
gtest_discover_tests(power_up_test)

add_executable(enemy_test enemy_test.cpp)
target_link_libraries(enemy_test PRIVATE core GTest::gmock_main)
gtest_discover_tests(enemy_test)

add_executable(death_test death_test.cpp)
target_link_libraries(death_test PRIVATE core GTest::gmock_main)
gtest_discover_tests(death_test)

add_executable(input_validation_test input_validation_test.cpp)
target_link_libraries(input_validation_test PRIVATE core GTest::gmock_main)
gtest_discover_tests(input_validation_test)

add_executable(fixed_timestep_test fixed_timestep_test.cpp)
target_link_libraries(fixed_timestep_test PRIVATE core GTest::gmock_main)
gtest_discover_tests(fixed_timestep_test)
