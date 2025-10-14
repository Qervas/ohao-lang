#pragma once

// Platform detection macros for conditional compilation
// Use these to write platform-specific code cleanly

#ifdef Q_OS_MACOS
    #define PLATFORM_MACOS 1
    #define PLATFORM_NAME "macOS"
#elif defined(Q_OS_WIN)
    #define PLATFORM_WINDOWS 1
    #define PLATFORM_NAME "Windows"
#elif defined(Q_OS_LINUX)
    #define PLATFORM_LINUX 1
    #define PLATFORM_NAME "Linux"
#endif

// OCR capabilities per platform
#ifdef PLATFORM_MACOS
    #define HAS_NATIVE_OCR 1
    #define NATIVE_OCR_NAME "Apple Vision"
#elif defined(PLATFORM_WINDOWS)
    #define HAS_NATIVE_OCR 0  // Windows OCR not yet implemented
    #define NATIVE_OCR_NAME "Windows OCR"
#else
    #define HAS_NATIVE_OCR 0
    #define NATIVE_OCR_NAME "None"
#endif
