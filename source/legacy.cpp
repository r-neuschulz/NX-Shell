// ============================================================================
// Legacy Global References Implementation
// ============================================================================
// This file provides the definitions for backwards-compatible global references
// that forward to the App service structs. Legacy API functions are implemented
// in their respective source files (fs.cpp, etc.) - this file only provides
// the global variable definitions and Config legacy API.
// ============================================================================

#include "config.hpp"
#include "gui.hpp"
#include "services.hpp"
#include "selection.hpp"

// ============================================================================
// Global forwarding references
// ============================================================================

// Config globals
ConfigData& cfg = GetApp().config.saved;
ConfigData& eff = GetApp().config.effective;

// Filesystem globals
std::string& cwd = GetApp().fs.cwd;
std::string& device = GetApp().fs.device;
FsFileSystem*& fs = GetApp().fs.current_fs;
FsFileSystem (&devices)[FileSystemMax] = GetApp().fs.devices;

// Window globals
WindowService& data = GetApp().window;
int& sort = GetApp().window.sort;

// Device registry globals
std::vector<std::string>& devices_list = GetApp().device_registry.devices;
std::recursive_mutex& devices_list_mutex = GetApp().device_registry.mutex;

// Texture globals
std::vector<Tex>& file_icons = GetApp().textures.file_icons;
Tex& folder_icon = GetApp().textures.folder_icon;
Tex& check_icon = GetApp().textures.check_icon;
Tex& uncheck_icon = GetApp().textures.uncheck_icon;
Tex& partcheck_icon = GetApp().textures.partcheck_icon;
Tex& drive_icon = GetApp().textures.drive_icon;
Tex& settings_icon = GetApp().textures.settings_icon;

// GUI globals (in GUI namespace)
namespace GUI {
    int& display_width = GetApp().gui.display_width;
    int& display_height = GetApp().gui.display_height;
}

// ============================================================================
// Legacy Config API (config.cpp no longer has these)
// ============================================================================

namespace Config {
    int Save(ConfigData &config) {
        App& app = GetApp();
        if (&config != &app.config.saved) {
            app.config.saved = config;
        }
        return Save(app.config, app.fs);
    }
    
    int Load(void) {
        App& app = GetApp();
        return Load(app.config, app.fs);
    }
    
    void UpdateEffective(void) {
        App& app = GetApp();
        UpdateEffective(app.config, GUI::IsAppletMode());
    }
    
    int GetLang(void) {
        return GetApp().config.effective.lang;
    }
    
    void SetLastDevice(const std::string& dev) {
        App& app = GetApp();
        SetLastDevice(app.config, dev, GUI::IsAppletMode());
    }
    
    void SetLastCwd(const std::string& path) {
        App& app = GetApp();
        SetLastCwd(app.config, path, GUI::IsAppletMode());
    }
}

// Note: Legacy FS API functions are implemented in fs.cpp
// Note: g_selection is defined in selection.cpp
