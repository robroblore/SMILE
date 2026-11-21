cmake_minimum_required(VERSION 3.21)

project(SMILE
    VERSION 0.3.0
    DESCRIPTION "Smart Meeting Interactive Live Education"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Multimedia Network)
qt_standard_project_setup()

qt_add_executable(smile WIN32 MACOSX_BUNDLE
    src/main.cpp

    # Core: session logic and data, no UI
    src/core/Tip.h
    src/core/Tip.cpp
    src/core/Agenda.h
    src/core/SessionReport.h
    src/core/TimeFormat.h
    src/core/TipSource.h
    src/core/DemoTipSource.h
    src/core/DemoTipSource.cpp
    src/core/SessionController.h
    src/core/SessionController.cpp
    src/core/AppSettings.h
    src/core/AppSettings.cpp

    # Sensors: camera and microphone (read-only, nothing is recorded)
    src/sensors/CameraMonitor.h
    src/sensors/CameraMonitor.cpp
    src/sensors/MicrophoneMonitor.h
    src/sensors/MicrophoneMonitor.cpp

    # AI: Together AI client and the smart tip sources
    src/ai/AiClient.h
    src/ai/AiClient.cpp
    src/ai/VisionTipSource.h
    src/ai/VisionTipSource.cpp
    src/ai/SpeechTipSource.h
    src/ai/SpeechTipSource.cpp

    # UI kit: Windows 11 style theme, icons and controls
    src/ui/Theme.h
    src/ui/Theme.cpp
    src/ui/Icons.h
    src/ui/Icons.cpp
    src/ui/Widgets.h
    src/ui/Widgets.cpp

    # Companion: the floating on-screen coach
    src/companion/SmileyPainter.h
    src/companion/SmileyPainter.cpp
    src/companion/CompanionWindow.h
    src/companion/CompanionWindow.cpp

    # Main app window and the four phase pages
    src/app/MainWindow.h
    src/app/MainWindow.cpp
    src/app/AgendaEditor.h
    src/app/AgendaEditor.cpp
    src/app/SessionRunWidget.h
    src/app/SessionRunWidget.cpp
    src/app/pages/TrainingPage.h
    src/app/pages/TrainingPage.cpp
    src/app/pages/PreMeetPage.h
    src/app/pages/PreMeetPage.cpp
    src/app/pages/MeetPage.h
    src/app/pages/MeetPage.cpp
    src/app/pages/PostMeetPage.h
    src/app/pages/PostMeetPage.cpp
    src/app/pages/SettingsPage.h
    src/app/pages/SettingsPage.cpp
)

target_include_directories(smile PRIVATE src)
target_compile_definitions(smile PRIVATE SMILE_VERSION="${PROJECT_VERSION}")
target_link_libraries(smile PRIVATE Qt6::Widgets Qt6::Multimedia Qt6::Network)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(smile PRIVATE -Wall -Wextra -Wpedantic)
elseif(MSVC)
    target_compile_options(smile PRIVATE /W4 /permissive-)
endif()

set_target_properties(smile PROPERTIES
    OUTPUT_NAME SMILE
    MACOSX_BUNDLE_BUNDLE_NAME SMILE
    MACOSX_BUNDLE_GUI_IDENTIFIER io.github.robroblore.smile
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    # Adds the camera and microphone usage descriptions macOS requires
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/Info.plist.in
)

include(GNUInstallDirs)
install(TARGETS smile
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(UNIX AND NOT APPLE)
    install(FILES packaging/linux/smile.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES packaging/linux/smile.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
endif()
