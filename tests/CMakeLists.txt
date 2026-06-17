
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
