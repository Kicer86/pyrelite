
find_package(GTest REQUIRED)

add_executable(grid_test grid_test.cpp)
target_link_libraries(grid_test PRIVATE core GTest::gmock_main)

include(GoogleTest)
gtest_discover_tests(grid_test)
