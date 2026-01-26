#pragma once

#include "imgui.h"
#include "services.hpp"

namespace GUI {
    // Display dimensions - forward to App::gui
    extern int& display_width;
    extern int& display_height;
    
    bool Init(void);
    bool SwapBuffers(void);
    bool Loop(u64 &key);
    void Render(void);
    void Exit(void);
    
    bool IsDocked(void);
    bool IsAppletMode(void);
    void UpdateDisplayDimensions(void);
    
    bool IsHoldingToClose(float &progress);
    void StartRefreshAnimation(void);
    bool IsRefreshAnimating(float &progress);
    bool WasPopupOpenOnBPress(void);
    
    void RenderStatsOverlay(void);
    void ResetUIState(void);
    void ResetImGuiSettings(void);
    
    void UpdateThemeColors(void);
    bool IsCurrentThemeDark(void);
    ImU32 GetThemeLabelColor(void);
    
    void UpdateAccentColors(void);
    ImU32 GetAccentColorU32(void);
    ImU32 GetAccentColorU32WithAlpha(int alpha);
    
    bool IsSystemThemeDark(void);
    
    ImU32 GetButtonColorA(void);
    ImU32 GetButtonColorB(void);
    ImU32 GetButtonColorX(void);
    ImU32 GetButtonColorY(void);
    ImU32 GetButtonColorPlus(void);
    ImU32 GetButtonColorMinus(void);
    ImU32 GetButtonTextColor(void);
    
    void SetRightStickScrollSuppressed(bool suppress);
}

namespace Toast {
    void DrawFilename(const char* text);
    void DrawCentered(const char* text, float alpha = 1.0f);
}
