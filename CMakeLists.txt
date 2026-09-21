cmake_minimum_required(VERSION 3.24)

project(StaxStudio
    VERSION 0.1.0
    DESCRIPTION "A lightweight Windows-first recording and streaming studio"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.5)
qt_policy(SET QTP0004 NEW)

qt_add_executable(StaxStudio
    WIN32
    app/main.cpp
    ui/controllers/AppController.cpp
    ui/controllers/AppController.h
)

qt_add_qml_module(StaxStudio
    URI StaxStudio
    VERSION 1.0
    QML_FILES
        ui/qml/Main.qml
        ui/qml/Home.qml
        ui/qml/Record.qml
        ui/qml/Stream.qml
        ui/qml/Library.qml
        ui/qml/Settings.qml
        ui/qml/components/ActionTile.qml
        ui/qml/components/NavigationRail.qml
)

target_include_directories(StaxStudio
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(StaxStudio
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
)

set_target_properties(StaxStudio PROPERTIES
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)

install(TARGETS StaxStudio
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
