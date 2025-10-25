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

# On macOS, enable Objective-C++ and add .mm files
if(APPLE)
    enable_language(OBJCXX)
    file(GLOB_RECURSE OBJCXX_SOURCES "src/*.mm")
    list(APPEND ALL_SOURCES ${OBJCXX_SOURCES})
    message(STATUS "Found ${CMAKE_MATCH_COUNT} Objective-C++ files for macOS")
endif()

# Create executable
# WIN32 flag creates Windows subsystem app (no console window)
# MACOSX_BUNDLE creates a macOS .app bundle
if(WIN32)
    qt_add_executable(ohao-lang WIN32 ${ALL_SOURCES} ${ALL_HEADERS})
elseif(APPLE)
    qt_add_executable(ohao-lang MACOSX_BUNDLE ${ALL_SOURCES} ${ALL_HEADERS})

    # Set proper rpath for macOS bundle - this is the standard way
    set_target_properties(ohao-lang PROPERTIES
        INSTALL_RPATH "@executable_path/../Frameworks"
        BUILD_WITH_INSTALL_RPATH TRUE
        MACOSX_RPATH TRUE
    )
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

# Link Apple frameworks on macOS for native OCR support and global shortcuts
if(APPLE)
    find_library(VISION_FRAMEWORK Vision)
    find_library(APPKIT_FRAMEWORK AppKit)
    find_library(COREIMAGE_FRAMEWORK CoreImage)
    find_library(FOUNDATION_FRAMEWORK Foundation)
    find_library(CARBON_FRAMEWORK Carbon)

    if(VISION_FRAMEWORK AND APPKIT_FRAMEWORK AND COREIMAGE_FRAMEWORK AND FOUNDATION_FRAMEWORK)
        target_link_libraries(ohao-lang PRIVATE
            ${VISION_FRAMEWORK}
            ${APPKIT_FRAMEWORK}
            ${COREIMAGE_FRAMEWORK}
            ${FOUNDATION_FRAMEWORK}
        )
        target_compile_definitions(ohao-lang PRIVATE APPLE_VISION_AVAILABLE)
        message(STATUS "Apple Vision framework found - native OCR support enabled")
    else()
        message(WARNING "Apple Vision framework not found - native OCR will be disabled")
    endif()

    if(CARBON_FRAMEWORK)
        target_link_libraries(ohao-lang PRIVATE ${CARBON_FRAMEWORK})
        message(STATUS "Carbon framework found - global shortcuts enabled")
    else()
        message(WARNING "Carbon framework not found - global shortcuts will be disabled")
    endif()
endif()

# Link X11 libraries on Linux for global shortcuts
if(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    if(X11_FOUND)
        target_link_libraries(ohao-lang PRIVATE ${X11_LIBRARIES} ${X11_Xext_LIB})
        target_include_directories(ohao-lang PRIVATE ${X11_INCLUDE_DIR})
        message(STATUS "X11 libraries found - global shortcuts enabled")
    else()
        message(WARNING "X11 libraries not found - global shortcuts will be disabled")
    endif()
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

    # Copy resources folder to build directory
    add_custom_command(TARGET ohao-lang POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/resources"
                "$<TARGET_FILE_DIR:ohao-lang>/resources"
        COMMENT "Copying resources folder to build directory")
endif()

# Deploy Qt runtime on macOS and create DMG
if(APPLE)
    # Set bundle properties
    set_target_properties(ohao-lang PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.ohao.lang"
        MACOSX_BUNDLE_BUNDLE_NAME "Ohao Lang"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
        MACOSX_BUNDLE_INFO_STRING "OCR & Translation Tool"
        MACOSX_BUNDLE_ICON_FILE "AppIcon.icns"
    )

    # Copy icon to Resources folder
    set(ICON_FILE "${CMAKE_SOURCE_DIR}/resources/AppIcon.icns")
    if(EXISTS ${ICON_FILE})
        set_source_files_properties(${ICON_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
        target_sources(ohao-lang PRIVATE ${ICON_FILE})
        message(STATUS "App icon found and will be included in bundle")
    else()
        message(WARNING "App icon not found at ${ICON_FILE}")
    endif()

    # Copy resources folder to bundle
    add_custom_command(TARGET ohao-lang POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/resources"
                "$<TARGET_BUNDLE_DIR:ohao-lang>/Contents/Resources/resources"
        COMMENT "Copying resources folder to macOS bundle")

    # Find macdeployqt
    get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
    find_program(MACDEPLOYQT_EXECUTABLE macdeployqt HINTS "${_qt_bin_dir}")

    if(MACDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET ohao-lang POST_BUILD
            COMMAND "${MACDEPLOYQT_EXECUTABLE}"
                    "$<TARGET_BUNDLE_DIR:ohao-lang>"
                    -always-overwrite
            COMMAND ${CMAKE_COMMAND} -E echo "Code signing the bundle..."
            # Sign the entire bundle (macdeployqt already signs components, we just ensure bundle signature)
            COMMAND codesign
                    --force
                    --deep
                    --sign -
                    "$<TARGET_BUNDLE_DIR:ohao-lang>"
            COMMAND ${CMAKE_COMMAND} -E echo "✅ Bundle ready"
            COMMENT "Deploying Qt frameworks and signing the app bundle")
        message(STATUS "macdeployqt found - will bundle Qt frameworks and sign automatically")

        # Add custom target to create DMG (run manually with: cmake --build build --target dmg)
        add_custom_target(dmg
            COMMAND "${MACDEPLOYQT_EXECUTABLE}"
                    "$<TARGET_BUNDLE_DIR:ohao-lang>"
                    -dmg
            DEPENDS ohao-lang
            COMMENT "Creating DMG installer..."
        )
        message(STATUS "To create DMG installer, run: cmake --build build --target dmg")
    else()
        message(WARNING "macdeployqt not found. The app bundle may not run on other systems without Qt installed.")
    endif()
endif()

# Copy resources for Linux
if(UNIX AND NOT APPLE)
    add_custom_command(TARGET ohao-lang POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/resources"
                "$<TARGET_FILE_DIR:ohao-lang>/resources"
        COMMENT "Copying resources folder to build directory")
endif()
