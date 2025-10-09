cmake_minimum_required(VERSION 3.16)
project(OhaoLang VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Auto-detect Qt installation on Windows
if(WIN32 AND NOT CMAKE_PREFIX_PATH)
    # Check if Qt is in C:/Qt
    if(EXISTS "C:/Qt")
        # Find Qt 6.x version
        file(GLOB QT_VERSIONS "C:/Qt/6.*")
        if(QT_VERSIONS)
            # Get the latest version
            list(SORT QT_VERSIONS ORDER DESCENDING)
            list(GET QT_VERSIONS 0 QT_VERSION_PATH)
            get_filename_component(QT_VERSION ${QT_VERSION_PATH} NAME)

            # Check for compiler directories in order of preference
            if(EXISTS "C:/Qt/${QT_VERSION}/msvc2022_64")
                set(CMAKE_PREFIX_PATH "C:/Qt/${QT_VERSION}/msvc2022_64")
            elseif(EXISTS "C:/Qt/${QT_VERSION}/msvc2019_64")
                set(CMAKE_PREFIX_PATH "C:/Qt/${QT_VERSION}/msvc2019_64")
            elseif(EXISTS "C:/Qt/${QT_VERSION}/mingw_64")
                set(CMAKE_PREFIX_PATH "C:/Qt/${QT_VERSION}/mingw_64")
            endif()

            if(CMAKE_PREFIX_PATH)
                message(STATUS "Auto-detected Qt at: ${CMAKE_PREFIX_PATH}")
            endif()
        endif()
    endif()
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui DBus Network Multimedia)
find_package(Qt6 COMPONENTS TextToSpeech)

qt_standard_project_setup()

# Recursively find all source and header files
file(GLOB_RECURSE ALL_SOURCES
    "src/*.cpp"
)

file(GLOB_RECURSE ALL_HEADERS
    "src/*.h"
)

# Create executable
# WIN32 flag creates Windows subsystem app (no console window)
if(WIN32)
    qt_add_executable(ohao-lang WIN32 ${ALL_SOURCES} ${ALL_HEADERS})
else()
    qt_add_executable(ohao-lang ${ALL_SOURCES} ${ALL_HEADERS})
endif()

# Add include directories - main src and key subdirectories
target_include_directories(ohao-lang PRIVATE
    src/
    src/ocr/
    src/translation/
    src/system/
    src/capture/
    src/tts/
    src/ui/overlays/
)

# Optional: Allow enabling console for release builds via CMake option
option(ENABLE_CONSOLE "Enable console window for debugging" OFF)
if(ENABLE_CONSOLE AND WIN32)
    set_target_properties(ohao-lang PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:CONSOLE"
    )
endif()

target_link_libraries(ohao-lang PRIVATE Qt6::Core Qt6::Widgets Qt6::Gui Qt6::DBus Qt6::Network Qt6::Multimedia)

if(Qt6TextToSpeech_FOUND)
    target_link_libraries(ohao-lang PRIVATE Qt6::TextToSpeech)
    target_compile_definitions(ohao-lang PRIVATE QT_TEXTTOSPEECH_AVAILABLE)
else()
    message(WARNING "Qt6 TextToSpeech not found. TTS features will be disabled.")
endif()

# Deploy Qt runtime on Windows so the .exe can run standalone
if (WIN32)
    # Try to locate windeployqt based on the detected/defined Qt path
    set(_qt_bin_dir "${CMAKE_PREFIX_PATH}/bin")
    if (EXISTS "${_qt_bin_dir}/windeployqt.exe")
        set(WINDEPLOYQT_EXE "${_qt_bin_dir}/windeployqt.exe")
    else()
        # Fallback: look relative to Qt6::CoreTools/qmake if available
        get_target_property(_qt_qmake_location Qt6::qmake IMPORTED_LOCATION)
        if (_qt_qmake_location)
            get_filename_component(_qt_tools_bin "${_qt_qmake_location}" DIRECTORY)
            if (EXISTS "${_qt_tools_bin}/windeployqt.exe")
                set(WINDEPLOYQT_EXE "${_qt_tools_bin}/windeployqt.exe")
            endif()
        endif()
    endif()

    if (WINDEPLOYQT_EXE)
        add_custom_command(TARGET ohao-lang POST_BUILD
            COMMAND "${WINDEPLOYQT_EXE}"
                    --$<IF:$<CONFIG:Debug>,debug,release>
                    --compiler-runtime
                    "$<TARGET_FILE:ohao-lang>"
            COMMENT "Deploying Qt runtime with windeployqt")
    else()
        message(WARNING "windeployqt.exe not found. The built .exe may not run outside the build tree without Qt DLLs in PATH.")
    endif()
endif()