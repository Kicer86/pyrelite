
find_package(Qt6 REQUIRED COMPONENTS Quick)

qt_standard_project_setup(REQUIRES 6.5)

# Refresh version.h (git SHA + build date) on every build, so the in-app
# version stamp always reflects the commit actually being built.
add_custom_target(pyrelite_version ALL
    BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/version.h
    COMMAND ${CMAKE_COMMAND}
        -DIN_FILE=${CMAKE_CURRENT_SOURCE_DIR}/version.h.in
        -DOUT_FILE=${CMAKE_CURRENT_BINARY_DIR}/version.h
        -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/GenerateVersion.cmake
    COMMENT "Updating version.h"
    VERBATIM
)

qt_add_executable(pyrelite
    main.cpp
    board_model.h
    board_model.cpp
)

add_dependencies(pyrelite pyrelite_version)
target_include_directories(pyrelite PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

qt_add_qml_module(pyrelite
    URI PyreliteApp
    VERSION 1.0
    QML_FILES
        Main.qml
        PlayerSprite.qml
)

target_link_libraries(pyrelite PRIVATE core Qt6::Quick)
