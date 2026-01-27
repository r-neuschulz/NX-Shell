#ifndef NX_SHELL_VERSION_HPP
#define NX_SHELL_VERSION_HPP

// =============================================================================
// NX-Shell Version Information
// =============================================================================
// All version macros (VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO, VERSION_STRING)
// are provided by CMake at compile time from CMakeLists.txt project() version.
//
// This header provides derived constants and utilities for consistent version
// usage across the codebase.
// =============================================================================

// Stringify helper macros (for building compile-time strings from numeric macros)
#define NX_SHELL_STRINGIFY(x) #x
#define NX_SHELL_TOSTRING(x) NX_SHELL_STRINGIFY(x)

// Pre-built version strings (compile-time constants)
// Use these instead of rebuilding from VERSION_MAJOR/MINOR/MICRO
#define NX_SHELL_VERSION_STR      "v" VERSION_STRING                    // "v5.0.0"
#define NX_SHELL_USER_AGENT       "NX-Shell/" VERSION_STRING            // "NX-Shell/5.0.0"
#define NX_SHELL_WINDOW_TITLE     "NX-Shell v" VERSION_STRING           // "NX-Shell v5.0.0"

// Integer version for numeric comparisons (e.g., update checks)
// Formula: major*10000 + minor*100 + micro (e.g., 5.12.3 -> 51203)
#define NX_SHELL_VERSION_INT      ((VERSION_MAJOR * 10000) + (VERSION_MINOR * 100) + VERSION_MICRO)

#endif // NX_SHELL_VERSION_HPP
