
find_package(GTest REQUIRED)

include(GoogleTest)

add_executable(grid_test grid_test.cpp)
target_link_libraries(grid_test PRIVATE core GTest::gmock_main)
gtest_discover_tests(grid_test)

add_executable(rng_test rng_test.cpp)
target_link_libraries(rng_test PRIVATE core GTest::gmock_main)
gtest_discover_tests(rng_test)
