cmake_minimum_required(VERSION 3.16)
project(OhaoLang VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui DBus)

qt_standard_project_setup()

set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/ScreenshotWidget.cpp
    src/SystemTray.cpp
    src/FloatingWidget.cpp
    src/SelectionToolbar.cpp
    src/ScreenCapture.cpp
    src/SettingsWindow.cpp
)

set(HEADERS
    src/MainWindow.h
    src/ScreenshotWidget.h
    src/SystemTray.h
    src/FloatingWidget.h
    src/SelectionToolbar.h
    src/ScreenCapture.h
    src/SettingsWindow.h
)

qt_add_executable(ohao-lang ${SOURCES} ${HEADERS})

target_link_libraries(ohao-lang PRIVATE Qt6::Core Qt6::Widgets Qt6::Gui Qt6::DBus)