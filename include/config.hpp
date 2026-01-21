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
    int lang = LANG_AUTO;
    bool dev_options = false;
    bool image_filename = false;
    bool enter_images_fullscreen = false;  // Open images in fullscreen mode by default
    int resolution_mode = ResolutionMode_Auto;
    int theme_mode = ThemeMode_Auto;  // UI theme: Auto, Dark, or Light
    bool show_details = false;
    bool show_stats = false;
    std::string last_device = "sdmc:";
    std::string last_cwd = "/";
    // Accent color stored as RGB floats (0.0 - 1.0)
    float accent_color[3] = {0.0f, 0.50f, 0.50f};  // Default: Teal (current theme color)
    int button_style = ButtonStyle_Colored;  // Navigation button style: Colored or Mono
} config_t;

extern config_t cfg;
extern std::string cwd;
extern std::string device;

namespace Config {
    int Save(config_t &config);
    int Load(void);
    int GetLang(void);  // Returns effective language (resolves LANG_AUTO to system language)
}
