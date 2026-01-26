#pragma once

#include <vector>

#include "imgui.h"
#include "services.hpp"

namespace BottomBar {
    // Button types that can appear in the bottom bar
    enum class ButtonType {
        Minus,      // Hold-to-close (always left-aligned, shows progress arc)
        Plus,       // Plus button (circle)
        CircleA,    // A button (red/mono circle)
        CircleB,    // B button (yellow/mono circle)
        CircleX,    // X button (blue/mono circle)
        CircleY,    // Y button (green/mono circle)
        ShoulderL,  // L shoulder button (chamfered polygon, left style)
        ShoulderR,  // R shoulder button (chamfered polygon, right style)
        ShoulderZR, // ZR trigger (outlined only, smaller text)
        DPadUp,     // Up arrow triangle
        DPadDown,   // Down arrow triangle
        RightStick, // Right stick circle with up/down arrows inside
    };

    // Configuration for a single hint item
    struct HintItem {
        ButtonType type;
        const char* label;                      // Text shown next to button
        bool active = false;                    // Highlighted state (accent color ring/outline)
    };

    // Configuration for the entire bottom bar
    struct Config {
        bool use_foreground_draw_list = false;  // true = overlay on screen, false = within window
        bool draw_background = false;           // Draw semi-transparent bar background
        float bar_height = 45.0f;               // Height of the bottom bar
        float button_radius = 12.0f;            // Radius for circle buttons
        float hint_spacing = 20.0f;             // Spacing between hint items
    };

    // Main drawing function
    // - cfg_svc: Config service for theme/button colors
    // - config: Bar configuration (draw list type, background, dimensions)
    // - left_items: Left-aligned items (typically just Minus button)
    // - right_items: Right-aligned items (main button hints)
    void Draw(ConfigService &cfg_svc, const Config& config, 
              const std::vector<HintItem>& left_items,
              const std::vector<HintItem>& right_items);
    
    // Constants for bar dimensions (exported for window sizing calculations)
    constexpr float DEFAULT_BAR_HEIGHT = 45.0f;
    constexpr float DEFAULT_BUTTON_RADIUS = 12.0f;
    constexpr float DEFAULT_HINT_SPACING = 20.0f;
}
