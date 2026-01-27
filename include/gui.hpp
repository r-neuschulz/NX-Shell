#pragma once

#include "imgui.h"
#include "services.hpp"

namespace GUI {
    // Display dimensions - access via inline functions (passed GUIService reference)
    inline int& display_width(GUIService &gui_svc) { return gui_svc.display_width; }
    inline int& display_height(GUIService &gui_svc) { return gui_svc.display_height; }
    
    // Get screen center as ImVec2 (for popup positioning at any resolution)
    inline ImVec2 GetScreenCenter(GUIService &gui_svc) {
        return ImVec2(gui_svc.display_width * 0.5f, gui_svc.display_height * 0.5f);
    }
    
    // Overload for convenience when App is available
    inline ImVec2 GetScreenCenter(App &app) {
        return ImVec2(app.gui.display_width * 0.5f, app.gui.display_height * 0.5f);
    }
    
    bool Init(App &app);
    bool SwapBuffers(void);
    bool Loop(App &app, u64 &key);
    void Render(void);
    void Exit(App &app);
    
    bool IsDocked(void);
    bool IsAppletMode(void);
    void UpdateDisplayDimensions(App &app);
    
    bool IsHoldingToClose(float &progress);
    void StartRefreshAnimation(void);
    bool IsRefreshAnimating(float &progress);
    bool WasPopupOpenOnBPress(void);
    
    void RenderStatsOverlay(App &app);
    void ResetUIState(App &app);
    void ResetImGuiSettings(FileSystemService &fs_svc);
    
    void UpdateThemeColors(ConfigService &config_svc);
    bool IsCurrentThemeDark(ConfigService &config_svc);
    ImU32 GetThemeLabelColor(ConfigService &config_svc);
    
    void UpdateAccentColors(ConfigService &config_svc);
    ImU32 GetAccentColorU32(ConfigService &config_svc);
    ImU32 GetAccentColorU32WithAlpha(ConfigService &config_svc, int alpha);
    
    bool IsSystemThemeDark(void);
    
    ImU32 GetButtonColorA(ConfigService &config_svc);
    ImU32 GetButtonColorB(ConfigService &config_svc);
    ImU32 GetButtonColorX(ConfigService &config_svc);
    ImU32 GetButtonColorY(ConfigService &config_svc);
    ImU32 GetButtonColorPlus(ConfigService &config_svc);
    ImU32 GetButtonColorMinus(ConfigService &config_svc);
    ImU32 GetButtonTextColor(ConfigService &config_svc);
    
    void SetRightStickScrollSuppressed(bool suppress);
}

namespace Toast {
    void DrawFilename(ConfigService &config_svc, const char* text);
    void DrawCentered(GUIService &gui_svc, const char* text, float alpha = 1.0f);
    
    // Timed toast with success/failure state (auto-fades and disappears)
    void Show(const char* message, bool success = true, float duration = 3.0f);
    void RenderTimed(App &app);
}

namespace Screenshot {
    // Take a screenshot at native resolution and save to SD card
    // Shows a toast notification on completion
    bool Capture(App &app);
}
