
find_package(Qt6 REQUIRED COMPONENTS Quick)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(pyrelite
    main.cpp
    board_model.h
    board_model.cpp
)

qt_add_qml_module(pyrelite
    URI PyreliteApp
    VERSION 1.0
    QML_FILES
        Main.qml
)

target_link_libraries(pyrelite PRIVATE core Qt6::Quick)
