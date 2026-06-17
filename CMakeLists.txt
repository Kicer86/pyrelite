
cmake_minimum_required(VERSION 3.21)

project(Pyrelite LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(PYRELITE_BUILD_APP "Build the Qt Quick app" ON)
option(BUILD_TESTING "Build the test suite" ON)

enable_testing()

add_subdirectory(core)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

if(PYRELITE_BUILD_APP)
    add_subdirectory(app)
endif()
