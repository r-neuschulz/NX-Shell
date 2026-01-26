#pragma once

// ============================================================================
// Legacy Global References
// ============================================================================
// This file provides backwards-compatible global references that forward to
// the App service structs. These are provided for gradual migration - new code
// should use App& directly.
//
// To use: #include "legacy.hpp" in files that need access to the legacy globals.
// For legacy function APIs, include the appropriate headers (config.hpp, fs.hpp).
// Goal: Eventually remove this file once all code uses App& directly.
// ============================================================================

#include "services.hpp"

// Config globals - forward to App::config
extern ConfigData& cfg;  // app.config.saved
extern ConfigData& eff;  // app.config.effective

// Filesystem globals - forward to App::fs
extern std::string& cwd;     // app.fs.cwd
extern std::string& device;  // app.fs.device
extern FsFileSystem*& fs;    // app.fs.current_fs
extern FsFileSystem (&devices)[FileSystemMax];  // app.fs.devices

// Window globals - forward to App::window
extern WindowService& data;  // app.window
extern int& sort;            // app.window.sort

// Device registry globals - forward to App::device_registry
extern std::vector<std::string>& devices_list;       // app.device_registry.devices
extern std::recursive_mutex& devices_list_mutex;     // app.device_registry.mutex

// Texture globals - forward to App::textures
extern std::vector<Tex>& file_icons;
extern Tex& folder_icon;
extern Tex& check_icon;
extern Tex& uncheck_icon;
extern Tex& partcheck_icon;
extern Tex& drive_icon;
extern Tex& settings_icon;

// GUI globals - forward to App::gui (in GUI namespace)
namespace GUI {
    extern int& display_width;   // app.gui.display_width
    extern int& display_height;  // app.gui.display_height
}

// Selection global - wraps App::selection (defined in selection.cpp)
class SelectionStore;
extern SelectionStore g_selection;
