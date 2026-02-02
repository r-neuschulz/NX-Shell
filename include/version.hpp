#ifndef NX_SHELL_VERSION_HPP
#define NX_SHELL_VERSION_HPP

// =============================================================================
// NX-Shell Version Information
// =============================================================================
// All version macros (VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO, VERSION_STRING)
// are provided by CMake at compile time from CMakeLists.txt project() version.
//
// NX_SHELL_RELEASE_BUILD is 1 for release builds (from CI), 0 for dev builds.
// GIT_COMMIT_HASH is the short git commit hash (7 chars) for dev builds.
//
// This header provides derived constants and utilities for consistent version
// usage across the codebase.
// =============================================================================

// Stringify helper macros (for building compile-time strings from numeric macros)
#define NX_SHELL_STRINGIFY(x) #x
#define NX_SHELL_TOSTRING(x) NX_SHELL_STRINGIFY(x)

// Version strings differ based on build type:
// - Release builds (from CI): "v5.0.0"
// - Dev builds (local):       "v5.0.0-dev (abc1234)"
#if NX_SHELL_RELEASE_BUILD
    #define NX_SHELL_IS_DEV_BUILD 0
    #define NX_SHELL_VERSION_STR      "v" VERSION_STRING
    #define NX_SHELL_WINDOW_TITLE     "NX-Shell v" VERSION_STRING
#else
    #define NX_SHELL_IS_DEV_BUILD 1
    #define NX_SHELL_VERSION_STR      "v" VERSION_STRING "-dev (" GIT_COMMIT_HASH ")"
    #define NX_SHELL_WINDOW_TITLE     "NX-Shell v" VERSION_STRING "-dev"
#endif

// User agent doesn't include dev suffix (for HTTP requests)
#define NX_SHELL_USER_AGENT       "NX-Shell/" VERSION_STRING

// Integer version for numeric comparisons (e.g., update checks)
// Formula: major*10000 + minor*100 + micro (e.g., 5.12.3 -> 51203)
#define NX_SHELL_VERSION_INT      ((VERSION_MAJOR * 10000) + (VERSION_MINOR * 100) + VERSION_MICRO)

#endif // NX_SHELL_VERSION_HPP
