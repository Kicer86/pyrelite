
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
    sound_player.h
    sound_player.cpp
    third_party/miniaudio_impl.cpp
)

add_dependencies(pyrelite pyrelite_version)
# third_party/miniaudio is a git submodule pinned at tag 0.11.22; its single
# header is on the include path so both sound_player.cpp and the implementation
# translation unit can pull it in as <miniaudio.h>.
target_include_directories(pyrelite PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    third_party/miniaudio)

qt_add_qml_module(pyrelite
    URI PyreliteApp
    VERSION 1.0
    QML_FILES
        AudioController.qml
        BombSprite.qml
        EnemySprite.qml
        FlameSprite.qml
        Main.qml
        PerkCrystalSprite.qml
        PlayerSprite.qml
        PowerUpSprite.qml
        TerrainTile.qml

    RESOURCES
        sounds/bomb_place.sfx
        sounds/enemy_down.sfx
        sounds/explosion.sfx
        sounds/game_over.sfx
        sounds/level_up.sfx
        sounds/pickup.sfx
        sounds/step.sfx
)

target_link_libraries(pyrelite PRIVATE core Qt6::Quick)

# miniaudio drives audio output on every target. On the desktop it dlopen()s the
# native backend (ALSA/PulseAudio, CoreAudio, WASAPI) so no audio -dev packages
# are needed; under Emscripten it routes through Web Audio with no extra link
# inputs. We only have to supply the system primitives each backend leans on.
if(NOT EMSCRIPTEN)
    find_package(Threads REQUIRED)
    target_link_libraries(pyrelite PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
endif()

if(APPLE)
    target_link_libraries(pyrelite PRIVATE
        "-framework CoreFoundation"
        "-framework CoreAudio"
        "-framework AudioToolbox")
elseif(UNIX AND NOT EMSCRIPTEN)
    target_link_libraries(pyrelite PRIVATE m)
endif()
