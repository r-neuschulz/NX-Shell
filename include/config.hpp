#pragma once

#include <string>
#include <switch.h>

// Resolution modes for display output
enum ResolutionMode {
    ResolutionMode_Auto = 0,  // Auto-detect based on dock state
    ResolutionMode_1080p = 1, // Force 1080p
    ResolutionMode_720p = 2   // Force 720p
};

// Theme modes for UI appearance
enum ThemeMode {
    ThemeMode_Auto = 0,   // Follow Nintendo system theme (dark/light)
    ThemeMode_Dark = 1,   // Force dark mode
    ThemeMode_Light = 2   // Force light mode
};

// Button style for navigation hints (A/B/X/Y)
enum ButtonStyle {
    ButtonStyle_Colored = 0,  // Colored buttons (default)
    ButtonStyle_Mono = 1,     // Black & white based on theme
    ButtonStyle_Accent = 2    // Accent color for all buttons
};

// Language setting (-1 = auto-detect from system)
constexpr int LANG_AUTO = -1;

typedef struct {
    // === Title mode settings (preserved when running in applet mode) ===
    int lang = LANG_AUTO;
    bool dev_options = false;
    bool image_filename = false;
    bool enter_images_fullscreen = false;
    int resolution_mode = ResolutionMode_Auto;
    int theme_mode = ThemeMode_Auto;
    bool show_details = false;
    bool show_stats = false;
    std::string last_device = "sdmc:";
    std::string last_cwd = "/";
    float accent_color[3] = {0.0f, 0.50f, 0.50f};  // Default: Teal
    int button_style = ButtonStyle_Colored;
    
    // === Applet mode settings (separate from title mode) ===
    bool applet_dev_options = false;
    std::string applet_last_device = "sdmc:";
    std::string applet_last_cwd = "/";
} config_t;

// cfg = saved config, eff = effective config (use eff for reading!)
// In applet mode: eff uses forced defaults (English, 720p, Dark, Mono, etc.)
// In title mode: eff copies from cfg
extern config_t cfg;
extern config_t eff;
extern std::string cwd;
extern std::string device;

namespace Config {
    int Save(config_t &config);
    int Load(void);
    void UpdateEffective(void);  // Call at startup and when applet mode might change
    int GetLang(void);           // Returns eff.lang (for string lookups)
    
    // Navigation - writes to cfg.last_* or cfg.applet_last_* based on mode
    void SetLastDevice(const std::string& device);
    void SetLastCwd(const std::string& cwd);
}
