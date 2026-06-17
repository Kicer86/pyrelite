
add_library(core STATIC
    grid.cpp
    rng.cpp
)

target_include_directories(core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
