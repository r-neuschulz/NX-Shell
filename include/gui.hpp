#pragma once

#include "imgui.h"

namespace GUI {
    // Display dimensions (changes based on docked/handheld mode)
    extern int display_width;
    extern int display_height;
    
    bool Init(void);
    bool SwapBuffers(void);
    bool Loop(u64 &key);
    void Render(void);
    void Exit(void);
    
    // Returns true if console is docked (1080p), false if handheld (720p)
    bool IsDocked(void);
    
    // Returns true if running in applet mode (limited memory, launched from album)
    bool IsAppletMode(void);
    
    // Updates display dimensions based on current dock state
    void UpdateDisplayDimensions(void);
    
    // Hold-to-close status: returns true if minus is being held, progress 0.0-1.0
    bool IsHoldingToClose(float &progress);
    
    // Refresh animation: starts a full-circle animation for visual feedback
    void StartRefreshAnimation(void);
    // Returns true if refresh animation is active, progress 0.0-1.0
    bool IsRefreshAnimating(float &progress);
    
    // Returns true if a popup/combo was open when B was pressed this frame
    // Used to prevent double-handling B (closing popup AND switching tabs)
    bool WasPopupOpenOnBPress(void);
    
    // Stats overlay (shows resolution, FPS, memory, temperatures)
    void RenderStatsOverlay(void);
    
    // Reset UI state (call after language change to force full redraw)
    void ResetUIState(void);
    
    // Reset ImGui settings (table column widths, etc.) to defaults
    void ResetImGuiSettings(void);
    
    // Theme functions - call UpdateThemeColors() after changing cfg.theme_mode
    void UpdateThemeColors(void);
    bool IsCurrentThemeDark(void);  // Returns true if current theme is dark (considers Auto mode)
    ImU32 GetThemeLabelColor(void);  // Returns appropriate label color for current theme
    
    // Accent color functions - call UpdateAccentColors() after changing cfg.accent_color
    void UpdateAccentColors(void);
    ImU32 GetAccentColorU32(void);
    ImU32 GetAccentColorU32WithAlpha(int alpha);
    
    // Returns true if the Nintendo system theme is set to dark mode
    bool IsSystemThemeDark(void);
    
    // Button color functions - returns colors based on cfg.button_style
    // ButtonStyle_Colored: Nintendo Switch standard colors
    // ButtonStyle_Mono: Theme-aware black/white colors
    ImU32 GetButtonColorA(void);  // Red (colored) or mono
    ImU32 GetButtonColorB(void);  // Yellow (colored) or mono
    ImU32 GetButtonColorX(void);  // Blue (colored) or mono
    ImU32 GetButtonColorY(void);  // Green (colored) or mono
    ImU32 GetButtonColorPlus(void);   // Gray (same for both styles)
    ImU32 GetButtonColorMinus(void);  // Gray (same for both styles)
    ImU32 GetButtonTextColor(void);   // White (colored) or contrasting color (mono)
}

// Toast overlay drawing utilities
namespace Toast {
    // Draw a filename toast at top-left corner (for image/text viewer filename display)
    void DrawFilename(const char* text);
    
    // Draw a centered notification toast near bottom (for temporary messages like "Press ZR to exit fullscreen")
    // alpha: 0.0-1.0 for fade-out effects
    void DrawCentered(const char* text, float alpha = 1.0f);
}