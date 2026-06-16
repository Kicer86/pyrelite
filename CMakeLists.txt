
cmake_minimum_required(VERSION 3.21)

project(Pyrelite LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

add_subdirectory(core)
add_subdirectory(tests)
add_subdirectory(app)
