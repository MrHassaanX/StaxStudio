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

# QQuickRhiItem exposes QRhi through QtGui's versioned private headers. Keep that dependency in ui/render.
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui GuiPrivate Qml Quick QuickControls2 Multimedia ShaderTools Test)
qt_standard_project_setup(REQUIRES 6.5)
qt_policy(SET QTP0004 NEW)

qt_add_executable(StaxStudio
    WIN32
    app/main.cpp
    core/capture/CaptureTypes.cpp
    core/capture/CaptureTypes.h
    core/capture/VisualSourceManager.cpp
    core/capture/VisualSourceManager.h
    core/capture/windows/DxgiDesktopCapture.cpp
    core/capture/windows/DxgiDesktopCapture.h
    core/audio/AudioProcessing.cpp
    core/audio/AudioProcessing.h
    core/audio/AudioProcessing.cpp
    core/audio/AudioProcessing.h
    core/audio/AudioTypes.h
    core/audio/AudioInputManager.cpp
    core/audio/AudioInputManager.h
    core/audio/wasapi/WasapiAudioSource.cpp
    core/audio/wasapi/WasapiAudioSource.h
    core/audio/mixer/MixerChannel.h
    core/project/StudioProject.cpp
    core/project/StudioProject.h
    core/project/StudioRepository.cpp
    core/project/StudioRepository.h
    core/render/CompositorScene.cpp
    core/render/CompositorScene.h
    core/render/ProgramResolution.cpp
    core/render/ProgramResolution.h
    core/render/VisualSourceFrame.h
    core/render/shaders/compositor.frag
    core/render/shaders/compositor.vert
    core/scene/Scene.h
    core/source/Source.cpp
    core/source/Source.h
    core/transition/TransitionSettings.h
    ui/controllers/AppController.cpp
    ui/controllers/AppController.h
    ui/controllers/StudioController.cpp
    ui/controllers/StudioController.h
    ui/controllers/StudioModels.cpp
    ui/controllers/StudioModels.h
    ui/render/ProgramPreview.cpp
    ui/render/ProgramPreview.h
)

qt_add_shaders(StaxStudio "StaxCompositorShaders"
    PREFIX "/stax/shaders"
    FILES
        core/render/shaders/compositor.vert
        core/render/shaders/compositor.frag
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
        ui/qml/Studio.qml
        ui/qml/components/ActionTile.qml
        ui/qml/components/AudioMeter.qml
        ui/qml/components/IconButton.qml
        ui/qml/components/MixerChannel.qml
        ui/qml/components/PanelHeader.qml
        ui/qml/components/NavigationRail.qml
        ui/qml/components/SceneRow.qml
        ui/qml/components/SourceRow.qml
        ui/qml/components/StudioButton.qml
        ui/qml/components/StudioIcon.qml
        ui/qml/components/StudioPanel.qml
        ui/qml/components/StudioSectionHeader.qml
        ui/qml/components/StudioComboBox.qml
        ui/qml/components/StudioDialog.qml
        ui/qml/components/StudioMenu.qml
        ui/qml/components/StudioMenuItem.qml
        ui/qml/components/StudioSlider.qml
        ui/qml/components/StudioTextField.qml
        ui/qml/components/TransformDialog.qml
)

target_include_directories(StaxStudio
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(StaxStudio
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::GuiPrivate
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Multimedia
        ole32
        uuid
)

if(WIN32)
    add_custom_command(TARGET StaxStudio POST_BUILD
        COMMAND "$<TARGET_FILE:Qt6::windeployqt>"
            --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/ui/qml"
            "$<TARGET_FILE:StaxStudio>"
        COMMENT "Deploying Qt runtime dependencies for StaxStudio"
        VERBATIM
    )
endif()

set_target_properties(StaxStudio PROPERTIES
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)

install(TARGETS StaxStudio
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

enable_testing()

qt_add_executable(StaxStudioTests
    tests/StudioProjectTests.cpp
    core/audio/AudioProcessing.cpp
    core/audio/AudioProcessing.h
    core/audio/AudioProcessing.cpp
    core/audio/AudioProcessing.h
    core/capture/CaptureTypes.cpp
    core/project/StudioProject.cpp
    core/project/StudioProject.h
    core/project/StudioRepository.cpp
    core/project/StudioRepository.h
    core/render/CompositorScene.cpp
    core/render/CompositorScene.h
    core/render/ProgramResolution.cpp
    core/render/ProgramResolution.h
    core/render/VisualSourceFrame.h
    core/scene/Scene.h
    core/source/Source.cpp
    core/source/Source.h
    core/transition/TransitionSettings.h
    core/audio/AudioProcessing.cpp
    core/audio/AudioProcessing.h
    core/audio/AudioProcessing.cpp
    core/audio/AudioProcessing.h
    core/audio/AudioTypes.h
    core/audio/AudioInputManager.cpp
    core/audio/AudioInputManager.h
    core/audio/wasapi/WasapiAudioSource.cpp
    core/audio/wasapi/WasapiAudioSource.h
    core/audio/mixer/MixerChannel.h
    ui/controllers/StudioModels.cpp
    ui/controllers/StudioModels.h
)

target_include_directories(StaxStudioTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(StaxStudioTests PRIVATE Qt6::Core Qt6::Gui Qt6::Test ole32 uuid propsys)

if(WIN32)
    add_custom_command(TARGET StaxStudioTests POST_BUILD
        COMMAND "$<TARGET_FILE:Qt6::windeployqt>"
            "$<TARGET_FILE:StaxStudioTests>"
        COMMENT "Deploying Qt runtime dependencies for StaxStudio tests"
        VERBATIM
    )
endif()

add_test(NAME StaxStudioTests COMMAND StaxStudioTests)

qt_add_executable(StaxStudioUiTests
    tests/StudioUiTests.cpp
    core/audio/AudioProcessing.cpp
    core/audio/AudioProcessing.h
    core/audio/AudioProcessing.cpp
    core/audio/AudioProcessing.h
    core/audio/AudioTypes.h
    core/audio/AudioProcessing.cpp
    core/audio/AudioProcessing.h
    core/audio/AudioProcessing.cpp
    core/audio/AudioProcessing.h
    core/audio/AudioInputManager.cpp
    core/audio/AudioInputManager.h
    core/audio/wasapi/WasapiAudioSource.cpp
    core/audio/wasapi/WasapiAudioSource.h
    core/capture/CaptureTypes.cpp
    core/capture/VisualSourceManager.cpp
    core/capture/windows/DxgiDesktopCapture.cpp
    core/project/StudioProject.cpp
    core/project/StudioRepository.cpp
    core/render/CompositorScene.cpp
    core/render/ProgramResolution.cpp
    core/source/Source.cpp
    ui/controllers/AppController.cpp
    ui/controllers/StudioController.cpp
    ui/controllers/StudioModels.cpp
    ui/render/ProgramPreview.cpp
)
target_include_directories(StaxStudioUiTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(StaxStudioUiTests PRIVATE STAX_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(StaxStudioUiTests PRIVATE Qt6::Core Qt6::Gui Qt6::GuiPrivate Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Multimedia Qt6::Test ole32 uuid propsys)
add_test(NAME StaxStudioUiTests COMMAND StaxStudioUiTests -o -,txt)
set_tests_properties(StaxStudioUiTests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_PLUGIN_PATH=$<TARGET_FILE_DIR:Qt6::Core>/../plugins;QT_QPA_FONTDIR=$ENV{WINDIR}/Fonts"
    TIMEOUT 60
)
