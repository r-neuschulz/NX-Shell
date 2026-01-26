#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <glad/glad.h>
#include <cstdio>
#include <cstring>
#include <memory>
#include <switch.h>

#include "config.hpp"
#include "fs.hpp"
#include "gui.hpp"
#include "language.hpp"
#include "services.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_switch.hpp"
#include "log.hpp"
#include "windows.hpp"

namespace GUI {
    // Note: display_width() and display_height() are now inline functions in gui.hpp
    // to avoid static initialization order fiasco
    
    static EGLDisplay s_display = EGL_NO_DISPLAY;
    static EGLContext s_context = EGL_NO_CONTEXT;
    static EGLSurface s_surface = EGL_NO_SURFACE;
    static EGLConfig s_config = nullptr;
    static NWindow *s_window = nullptr;
    static AppletOperationMode s_operation_mode = AppletOperationMode_Handheld;
    
    // Hold-to-close state
    static PadState s_pad;
    static bool s_pad_initialized = false;
    static u64 s_minus_hold_start = 0;
    static bool s_minus_is_held = false;
    static constexpr float HOLD_TO_CLOSE_SECONDS = 0.8f;
    
    // Right stick scroll suppression (for color picker etc. that use R stick for custom control)
    static bool s_right_stick_scroll_suppressed = false;
    
    // Refresh animation state (full circle animation for visual feedback)
    static u64 s_refresh_anim_start = 0;
    static bool s_refresh_anim_active = false;
    static constexpr float REFRESH_ANIM_SECONDS = 0.4f;  // Duration of full circle animation
    
    // Track popup state when B is pressed (to avoid double-handling B for popup close + tab switch)
    static bool s_popup_was_open_on_b_press = false;
    
    // Track theme state for auto-refresh when system theme changes
    static bool s_last_theme_dark = true;
    
    // Track when surface is recreated to reapply vsync setting after swap
    static bool s_surface_recreated = false;
    
    // Display dimensions accessed via GetApp().gui
    
    bool IsDocked(void) {
        return s_operation_mode == AppletOperationMode_Console;
    }
    
    bool IsAppletMode(void) {
        AppletType type = appletGetAppletType();
        // Application type means full memory access (title override or installed NSP)
        // Any other type (LibraryApplet, SystemApplet, etc.) means applet mode with limited memory
        return type != AppletType_Application;
    }
    
    // Recreate the EGL surface for new dimensions (needed for runtime resolution changes)
    static bool RecreateSurface(App &app) {
        if (!s_display || !s_window || !s_config)
            return false;
        
        // Unbind current surface
        eglMakeCurrent(s_display, EGL_NO_SURFACE, EGL_NO_SURFACE, s_context);
        
        // Destroy old surface
        if (s_surface) {
            eglDestroySurface(s_display, s_surface);
            s_surface = EGL_NO_SURFACE;
        }
        
        // Update native window dimensions
        nwindowSetDimensions(s_window, app.gui.display_width, app.gui.display_height);
        
        // Create new surface with updated dimensions
        s_surface = eglCreateWindowSurface(s_display, s_config, s_window, nullptr);
        if (!s_surface) {
            Log::Error("Surface recreation failed! error: %d", eglGetError());
            return false;
        }
        
        // Rebind the new surface
        eglMakeCurrent(s_display, s_surface, s_surface, s_context);
        
        // Re-apply VSync setting for uncapped frame rate (must be done after surface bind)
        eglSwapInterval(s_display, 0);
        
        // Mark that we need to reapply swap interval after the first buffer swap
        // (some drivers only honor the setting after a swap has occurred)
        s_surface_recreated = true;
        
        // Update the GL viewport to match new dimensions
        glViewport(0, 0, app.gui.display_width, app.gui.display_height);
        
        return true;
    }
    
    void UpdateDisplayDimensions(App &app) {
        AppletOperationMode mode = appletGetOperationMode();
        s_operation_mode = mode;
        
        // Determine target resolution based on config setting
        int target_width = 1280;
        int target_height = 720;
        
        switch (app.config.ResolutionMode()) {
            case ResolutionMode_Auto:
                // Auto-detect based on dock state
                if (IsDocked()) {
                    target_width = 1920;
                    target_height = 1080;
                } else {
                    target_width = 1280;
                    target_height = 720;
                }
                break;
                
            case ResolutionMode_1080p:
                target_width = 1920;
                target_height = 1080;
                break;
                
            case ResolutionMode_720p:
            default:
                target_width = 1280;
                target_height = 720;
                break;
        }
        
        // Only update if dimensions changed
        if (app.gui.display_width != target_width || app.gui.display_height != target_height) {
            app.gui.display_width = target_width;
            app.gui.display_height = target_height;
            
            // Recreate the EGL surface for the new resolution
            RecreateSurface(app);
        }
    }
    
    static bool InitEGL(App &app, NWindow* win) {
        s_window = win;
        
        // Check initial dock state and set dimensions accordingly
        s_operation_mode = appletGetOperationMode();
        if (IsDocked()) {
            app.gui.display_width = 1920;
            app.gui.display_height = 1080;
        } else {
            app.gui.display_width = 1280;
            app.gui.display_height = 720;
        }
        
        // Set native window dimensions to match current mode
        nwindowSetDimensions(win, app.gui.display_width, app.gui.display_height);
        
        s_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        
        if (!s_display) {
            Log::Error("Could not connect to display! error: %d", eglGetError());
            return false;
        }
        
        eglInitialize(s_display, nullptr, nullptr);
        
        if (eglBindAPI(EGL_OPENGL_API) == EGL_FALSE) {
            Log::Error("Could not set API! error: %d", eglGetError());
            eglTerminate(s_display);
            s_display = nullptr;
        }
        
        EGLint num_configs;
        static const EGLint framebuffer_attr_list[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
            EGL_RED_SIZE,     8,
            EGL_GREEN_SIZE,   8,
            EGL_BLUE_SIZE,    8,
            EGL_ALPHA_SIZE,   8,
            EGL_DEPTH_SIZE,   24,
            EGL_STENCIL_SIZE, 8,
            EGL_NONE
        };
        
        eglChooseConfig(s_display, framebuffer_attr_list, std::addressof(s_config), 1, std::addressof(num_configs));
        if (num_configs == 0) {
            Log::Error("No config found! error: %d", eglGetError());
            eglTerminate(s_display);
            s_display = nullptr;
        }
        
        s_surface = eglCreateWindowSurface(s_display, s_config, win, nullptr);
        if (!s_surface) {
            Log::Error("Surface creation failed! error: %d", eglGetError());
            eglTerminate(s_display);
            s_display = nullptr;
        }
        
        static const EGLint context_attr_list[] = {
            EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
            EGL_CONTEXT_MAJOR_VERSION_KHR, 4,
            EGL_CONTEXT_MINOR_VERSION_KHR, 3,
            EGL_NONE
        };
        
        s_context = eglCreateContext(s_display, s_config, EGL_NO_CONTEXT, context_attr_list);
        if (!s_context) {
            Log::Error("Context creation failed! error: %d", eglGetError());
            eglDestroySurface(s_display, s_surface);
            s_surface = nullptr;
        }
        
        eglMakeCurrent(s_display, s_surface, s_surface, s_context);
        
        // Disable VSync for uncapped frame rate
        eglSwapInterval(s_display, 0);
        
        return true;
    }
    
    static void ExitEGL(void) {
        if (s_display) {
            // Ensure all GL commands are finished before destroying context
            // This prevents crashes from in-flight GPU operations
            glFinish();
            
            eglMakeCurrent(s_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            
            if (s_context) {
                eglDestroyContext(s_display, s_context);
                s_context = nullptr;
            }
            
            if (s_surface) {
                eglDestroySurface(s_display, s_surface);
                s_surface = nullptr;
            }
            
            eglTerminate(s_display);
            s_display = nullptr;
            s_config = nullptr;
        }
    }

    bool SwapBuffers(void) {
        bool result = eglSwapBuffers(s_display, s_surface);
        
        // After a surface recreation, reapply swap interval after the first successful swap
        // This ensures the driver has fully initialized the new surface before we configure it
        if (s_surface_recreated && result) {
            eglSwapInterval(s_display, 0);
            s_surface_recreated = false;
        }
        
        return result;
    }

    bool IsCurrentThemeDark(ConfigService &config_svc) {
        switch (config_svc.ThemeMode()) {
            case ThemeMode_Auto:
                return IsSystemThemeDark();
            case ThemeMode_Dark:
                return true;
            case ThemeMode_Light:
                return false;
            default:
                return true;  // default to dark
        }
    }
    
    ImU32 GetThemeLabelColor(ConfigService &config_svc) {
        // Light gray for dark theme, dark gray for light theme
        return IsCurrentThemeDark(config_svc) 
            ? IM_COL32(200, 200, 200, 255)   // Light gray on dark background
            : IM_COL32(60, 60, 65, 255);     // Dark gray on light background
    }
    
    void UpdateThemeColors(ConfigService &config_svc) {
        ImVec4 *colors = ImGui::GetStyle().Colors;
        
        // Determine if we should use dark or light theme
        bool use_dark = IsCurrentThemeDark(config_svc);
        
        if (use_dark) {
            // Dark theme colors (background and text)
            colors[ImGuiCol_Text] = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
            colors[ImGuiCol_TextDisabled] = ImVec4(0.36f, 0.42f, 0.47f, 1.00f);
            colors[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
            colors[ImGuiCol_ChildBg] = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
            colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
            colors[ImGuiCol_Border] = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
            colors[ImGuiCol_FrameBg] = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
            colors[ImGuiCol_FrameBgHovered] = ImVec4(0.32f, 0.32f, 0.32f, 1.00f);
            colors[ImGuiCol_FrameBgActive] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
            colors[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.12f, 0.14f, 0.65f);
            colors[ImGuiCol_TitleBgActive] = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
            colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
            colors[ImGuiCol_MenuBarBg] = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
            colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.39f);
            colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
            colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
            colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
            colors[ImGuiCol_Button] = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
            colors[ImGuiCol_Header] = ImVec4(0.24f, 0.24f, 0.24f, 0.55f);
            colors[ImGuiCol_Separator] = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
            colors[ImGuiCol_Tab] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
            colors[ImGuiCol_TabDimmed] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
            colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
            // Table colors
            colors[ImGuiCol_TableHeaderBg] = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
            colors[ImGuiCol_TableBorderStrong] = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
            colors[ImGuiCol_TableBorderLight] = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
            colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
            colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
            // Modal dim: light overlay on dark background for contrast
            colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
        } else {
            // Light theme colors (background and text)
            colors[ImGuiCol_Text] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
            colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.52f, 0.55f, 1.00f);
            colors[ImGuiCol_WindowBg] = ImVec4(0.94f, 0.94f, 0.96f, 1.00f);
            colors[ImGuiCol_ChildBg] = ImVec4(0.98f, 0.98f, 0.99f, 1.00f);
            colors[ImGuiCol_PopupBg] = ImVec4(0.98f, 0.98f, 0.98f, 0.96f);
            colors[ImGuiCol_Border] = ImVec4(0.80f, 0.82f, 0.85f, 1.00f);
            colors[ImGuiCol_FrameBg] = ImVec4(0.89f, 0.89f, 0.89f, 1.00f);
            colors[ImGuiCol_FrameBgHovered] = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
            colors[ImGuiCol_FrameBgActive] = ImVec4(0.78f, 0.78f, 0.78f, 1.00f);
            colors[ImGuiCol_TitleBg] = ImVec4(0.88f, 0.88f, 0.90f, 0.65f);
            colors[ImGuiCol_TitleBgActive] = ImVec4(0.85f, 0.86f, 0.88f, 1.00f);
            colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.92f, 0.92f, 0.94f, 0.51f);
            colors[ImGuiCol_MenuBarBg] = ImVec4(0.90f, 0.90f, 0.92f, 1.00f);
            colors[ImGuiCol_ScrollbarBg] = ImVec4(0.92f, 0.92f, 0.94f, 0.39f);
            colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.72f, 0.72f, 0.72f, 1.00f);
            colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.62f, 0.62f, 0.62f, 1.00f);
            colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);
            colors[ImGuiCol_Button] = ImVec4(0.84f, 0.84f, 0.84f, 1.00f);
            colors[ImGuiCol_Header] = ImVec4(0.84f, 0.84f, 0.84f, 0.55f);
            colors[ImGuiCol_Separator] = ImVec4(0.77f, 0.77f, 0.77f, 1.00f);
            colors[ImGuiCol_Tab] = ImVec4(0.90f, 0.91f, 0.93f, 1.00f);
            colors[ImGuiCol_TabDimmed] = ImVec4(0.90f, 0.91f, 0.93f, 1.00f);
            colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.90f, 0.91f, 0.93f, 1.00f);
            // Table colors
            colors[ImGuiCol_TableHeaderBg] = ImVec4(0.85f, 0.86f, 0.88f, 1.00f);
            colors[ImGuiCol_TableBorderStrong] = ImVec4(0.70f, 0.72f, 0.75f, 1.00f);
            colors[ImGuiCol_TableBorderLight] = ImVec4(0.80f, 0.82f, 0.85f, 1.00f);
            colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
            colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.00f, 0.00f, 0.00f, 0.03f);
            // Modal dim: dark overlay on light background for contrast
            colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);
        }
    }
    
    static void SetDefaultTheme(ConfigService &config_svc) {
        ImGui::GetStyle().FrameRounding = 4.0f;
        ImGui::GetStyle().GrabRounding = 4.0f;
        
        // Initialize theme tracking to match current state
        s_last_theme_dark = IsCurrentThemeDark(config_svc);
        
        ImVec4 *colors = ImGui::GetStyle().Colors;
        
        // Theme-independent colors
        colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.37f, 0.61f, 1.00f, 1.00f);
        colors[ImGuiCol_SeparatorHovered] = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
        colors[ImGuiCol_SeparatorActive] = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
        colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.25f);
        colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
        colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
        colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
        colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
        colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
        colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
        
        // Apply theme-dependent colors (background, text, and modal dim)
        UpdateThemeColors(config_svc);
        
        // Apply accent colors from config
        UpdateAccentColors(config_svc);
    }
    
    void UpdateAccentColors(ConfigService &config_svc) {
        ImVec4 accent = ImVec4(config_svc.AccentR(), config_svc.AccentG(), config_svc.AccentB(), 1.0f);
        ImVec4 *colors = ImGui::GetStyle().Colors;
        
        colors[ImGuiCol_CheckMark] = accent;
        colors[ImGuiCol_ButtonHovered] = accent;
        colors[ImGuiCol_ButtonActive] = accent;
        colors[ImGuiCol_HeaderHovered] = accent;
        colors[ImGuiCol_HeaderActive] = accent;
        colors[ImGuiCol_TabHovered] = accent;
        colors[ImGuiCol_TabSelected] = accent;
        colors[ImGuiCol_PlotLines] = accent;
        colors[ImGuiCol_PlotHistogram] = accent;
        colors[ImGuiCol_NavCursor] = accent;
    }
    
    ImU32 GetAccentColorU32(ConfigService &config_svc) {
        return IM_COL32(
            static_cast<int>(config_svc.AccentR() * 255),
            static_cast<int>(config_svc.AccentG() * 255),
            static_cast<int>(config_svc.AccentB() * 255),
            255
        );
    }
    
    ImU32 GetAccentColorU32WithAlpha(ConfigService &config_svc, int alpha) {
        return IM_COL32(
            static_cast<int>(config_svc.AccentR() * 255),
            static_cast<int>(config_svc.AccentG() * 255),
            static_cast<int>(config_svc.AccentB() * 255),
            alpha
        );
    }
    
    bool IsSystemThemeDark(void) {
        ColorSetId color_set = ColorSetId_Light;
        if (R_SUCCEEDED(setsysGetColorSetId(&color_set))) {
            return color_set == ColorSetId_Dark;
        }
        // Default to dark if we can't read the setting
        return true;
    }
    
    // Nintendo Switch standard button colors (lookup table: A=Red, B=Yellow, X=Blue, Y=Green)
    static constexpr ImU32 kNintendoButtonColors[4] = {
        IM_COL32(235, 64, 52, 255),   // A - Red
        IM_COL32(200, 150, 0, 255),   // B - Yellow
        IM_COL32(65, 137, 230, 255),  // X - Blue
        IM_COL32(100, 180, 100, 255), // Y - Green
    };
    
    // Mono button color (theme-aware)
    static ImU32 GetMonoButtonColor(ConfigService &config_svc) {
        return IsCurrentThemeDark(config_svc) 
            ? IM_COL32(180, 180, 180, 255)   // Light gray on dark theme
            : IM_COL32(80, 80, 80, 255);     // Dark gray on light theme
    }
    
    // Plus/Minus button color (theme-aware, same for all styles)
    static ImU32 GetPlusMinusColor(ConfigService &config_svc) {
        return IsCurrentThemeDark(config_svc) 
            ? IM_COL32(80, 80, 80, 255)
            : IM_COL32(140, 140, 145, 255);
    }
    
    // Internal helper for ABXY button colors based on style
    static ImU32 GetABXYButtonColor(ConfigService &config_svc, int index) {
        int style = config_svc.ButtonStyle();
        
        if (style == ButtonStyle_Accent) {
            return GetAccentColorU32(config_svc);
        }
        if (style == ButtonStyle_Mono) {
            return GetMonoButtonColor(config_svc);
        }
        return kNintendoButtonColors[index];  // Colored style
    }
    
    // Button color functions - returns colors based on effective button style
    ImU32 GetButtonColorA(ConfigService &config_svc)     { return GetABXYButtonColor(config_svc, 0); }
    ImU32 GetButtonColorB(ConfigService &config_svc)     { return GetABXYButtonColor(config_svc, 1); }
    ImU32 GetButtonColorX(ConfigService &config_svc)     { return GetABXYButtonColor(config_svc, 2); }
    ImU32 GetButtonColorY(ConfigService &config_svc)     { return GetABXYButtonColor(config_svc, 3); }
    ImU32 GetButtonColorPlus(ConfigService &config_svc)  { return GetPlusMinusColor(config_svc); }
    ImU32 GetButtonColorMinus(ConfigService &config_svc) { return GetPlusMinusColor(config_svc); }
    
    ImU32 GetButtonTextColor(ConfigService &config_svc) {
        int style = config_svc.ButtonStyle();
        // Text on buttons - white for colored/accent style, contrasting for mono
        if (style == ButtonStyle_Mono) {
            return IsCurrentThemeDark(config_svc) 
                ? IM_COL32(40, 40, 40, 255)      // Dark text on light buttons (dark theme)
                : IM_COL32(240, 240, 240, 255);  // Light text on dark buttons (light theme)
        }
        if (style == ButtonStyle_Accent) {
            // Calculate luminance of accent color to determine text color
            float luminance = 0.299f * config_svc.AccentR() + 0.587f * config_svc.AccentG() + 0.114f * config_svc.AccentB();
            return luminance > 0.5f 
                ? IM_COL32(30, 30, 30, 255)      // Dark text on light accent
                : IM_COL32(255, 255, 255, 255);  // White text on dark accent
        }
        return IM_COL32(255, 255, 255, 255);  // White text on colored buttons
    }

    // Font requirement flags for different languages
    enum FontRequirement {
        FontReq_Standard = 0,            // Base font (all languages)
        FontReq_ChineseSimplified = 1,   // Simplified Chinese (lang 6)
        FontReq_ChineseTraditional = 2,  // Traditional Chinese (lang 11)
        FontReq_Korean = 4,              // Korean (lang 7)
    };
    
    // Determine which fonts are needed for a given language
    // Extensible: add new languages by adding cases here
    static int GetFontRequirements(int lang) {
        switch (lang) {
            case 6:  // Simplified Chinese
                return FontReq_ChineseSimplified;
            case 7:  // Korean
                return FontReq_Korean;
            case 11: // Traditional Chinese
                return FontReq_ChineseTraditional;
            default: // All other languages (jp, en, fr, de, it, es, nl, pt, ru)
                return FontReq_Standard;
        }
    }
    
    bool Init(App &app) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        (void)io;
        
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        
        // Set ImGui ini file path on SD card for persistent settings (table column widths, etc.)
        static const char* imgui_ini_path = "/switch/NX-Shell/imgui.ini";
        io.IniFilename = imgui_ini_path;
        
        // Try to load existing settings if the file exists
        ImGui::LoadIniSettingsFromDisk(imgui_ini_path);
        
        if (!InitEGL(app, nwindowGetDefault()))
            return false;
        
        gladLoadGL();
        
        ImGui_ImplSwitch_Init("#version 130");
        
        // Determine which fonts are needed based on current language setting
        int lang = app.config.Lang();
        int font_reqs = GetFontRequirements(lang);
        
        // Always load CJK fonts for proper display of all filenames/paths
        bool need_chinese_simplified = true;
        bool need_chinese_traditional = (font_reqs & FontReq_ChineseTraditional) != 0;
        bool need_korean = true;
        
        // Load nintendo fonts - always need Standard + NintendoExt
        PlFontData standard, extended;
        static ImWchar extended_range[] = {0xE000, 0xE152, 0};
        
        // CJK glyph ranges (only used when needed)
        static const ImWchar chinese_ranges[] = {
            0x0020, 0x00FF, // Basic Latin + Latin Supplement
            0x2000, 0x206F, // General Punctuation
            0x3000, 0x30FF, // CJK Symbols and Punctuation, Hiragana, Katakana
            0x31F0, 0x31FF, // Katakana Phonetic Extensions
            0xFF00, 0xFFEF, // Half-width and Full-width Forms
            0x4E00, 0x9FAF, // CJK Unified Ideographs
            0,
        };
        static const ImWchar korean_ranges[] = {
            0x0020, 0x00FF, // Basic Latin + Latin Supplement
            0x3131, 0x3163, // Korean Hangul Compatibility Jamo
            0xAC00, 0xD7A3, // Korean Hangul Syllables
            0,
        };
        
        // Always load Standard and NintendoExt fonts
        if (R_SUCCEEDED(plGetSharedFontByType(std::addressof(standard), PlSharedFontType_Standard)) &&
            R_SUCCEEDED(plGetSharedFontByType(std::addressof(extended), PlSharedFontType_NintendoExt))) {
                
            ImFontConfig font_cfg;
            font_cfg.FontDataOwnedByAtlas = false;
            
            // Base font (required)
            io.Fonts->AddFontFromMemoryTTF(standard.address, standard.size, 20.f, std::addressof(font_cfg), io.Fonts->GetGlyphRangesDefault());
            
            // Nintendo extended symbols (always needed for controller icons)
            font_cfg.MergeMode = true;
            io.Fonts->AddFontFromMemoryTTF(extended.address, extended.size, 20.f, std::addressof(font_cfg), extended_range);
            
            // Load CJK fonts only if needed (saves ~15-20ms on startup for non-CJK users)
            if (need_chinese_simplified || need_chinese_traditional) {
                PlFontData chinese;
                PlSharedFontType chinese_type = need_chinese_traditional 
                    ? PlSharedFontType_ChineseTraditional 
                    : PlSharedFontType_ChineseSimplified;
                    
                if (R_SUCCEEDED(plGetSharedFontByType(std::addressof(chinese), chinese_type))) {
                    io.Fonts->AddFontFromMemoryTTF(chinese.address, chinese.size, 20.f, std::addressof(font_cfg), chinese_ranges);
                }
                // Fallback: if Traditional Chinese font not available, try Simplified
                else if (need_chinese_traditional && 
                         R_SUCCEEDED(plGetSharedFontByType(std::addressof(chinese), PlSharedFontType_ChineseSimplified))) {
                    io.Fonts->AddFontFromMemoryTTF(chinese.address, chinese.size, 20.f, std::addressof(font_cfg), chinese_ranges);
                }
            }
            
            if (need_korean) {
                PlFontData korean;
                if (R_SUCCEEDED(plGetSharedFontByType(std::addressof(korean), PlSharedFontType_KO))) {
                    io.Fonts->AddFontFromMemoryTTF(korean.address, korean.size, 20.f, std::addressof(font_cfg), korean_ranges);
                }
            }
            
            io.Fonts->Flags |= ImFontAtlasFlags_NoPowerOfTwoHeight;
            // Font atlas is built automatically when needed by the new texture system
        }

        SetDefaultTheme(app.config);
        return true;
    }
    
    bool Loop(App &app, u64 &key) {
        if (!appletMainLoop())
            return false;
        
        // Initialize pad state for hold-to-close detection (once)
        if (!s_pad_initialized) {
            padInitializeDefault(&s_pad);
            s_pad_initialized = true;
        }
        
        // Update pad and check for hold-to-close (minus button held for 1 seconds)
        padUpdate(&s_pad);
        u64 buttons = padGetButtons(&s_pad);
        
        if (buttons & HidNpadButton_Minus) {
            if (!s_minus_is_held) {
                // Just started holding minus
                s_minus_is_held = true;
                s_minus_hold_start = armGetSystemTick();
            } else {
                // Check if held long enough (3 seconds)
                u64 elapsed_ticks = armGetSystemTick() - s_minus_hold_start;
                float elapsed_seconds = (elapsed_ticks * 625.0f / 12.0f) / 1000000000.0f;
                if (elapsed_seconds >= HOLD_TO_CLOSE_SECONDS) {
                    return false;  // Exit the app
                }
            }
        } else {
            // Minus button released, reset tracking
            s_minus_is_held = false;
            s_minus_hold_start = 0;
        }
        
        // Check for dock/undock and update resolution if needed
        UpdateDisplayDimensions(app);
        
        // Check if theme has changed (for Auto mode or manual changes) and refresh colors
        bool current_theme_dark = IsCurrentThemeDark(app.config);
        if (current_theme_dark != s_last_theme_dark) {
            s_last_theme_dark = current_theme_dark;
            UpdateThemeColors(app.config);
            UpdateAccentColors(app.config);
        }
        
        key = ImGui_ImplSwitch_NewFrame();
        
        // Apply gamepad scrolling (D-pad up/down held or right stick) to the focused nav window
        // Scroll accelerates the longer the input is held
        // Skip if suppressed (e.g., color picker uses R stick for hue control)
        float right_stick_scroll = ImGui_ImplSwitch_GetRightStickScrollY();
        if (right_stick_scroll != 0.0f && !s_right_stick_scroll_suppressed) {
            ImGuiContext& g = *GImGui;
            // Find the best window to scroll - prefer the nav window or hovered window
            ImGuiWindow* scroll_window = g.NavWindow;
            if (scroll_window) {
                // Walk up to find a scrollable parent if the nav window itself isn't scrollable
                while (scroll_window && scroll_window->ScrollMax.y <= 0.0f && scroll_window->ParentWindow) {
                    scroll_window = scroll_window->ParentWindow;
                }
                if (scroll_window && scroll_window->ScrollMax.y > 0.0f) {
                    float new_scroll_y = scroll_window->Scroll.y + right_stick_scroll;
                    new_scroll_y = ImClamp(new_scroll_y, 0.0f, scroll_window->ScrollMax.y);
                    scroll_window->Scroll.y = new_scroll_y;
                }
            }
        }
        
        // Handle B button for popup closing vs custom navigation
        // B button is NOT mapped in imgui_impl_switch.cpp to prevent ImGui from intercepting it
        // (FileBrowser uses B for parent directory, Settings/About use B to jump back to FileBrowser)
        // BUT when a popup (including combo dropdown) is open, we need to send B to ImGui to close it
        if (key & HidNpadButton_B) {
            ImGuiContext& g = *GImGui;
            // Track if a popup was open when B was pressed - used by window.cpp to avoid double-handling
            s_popup_was_open_on_b_press = (g.OpenPopupStack.Size > 0);
            // If a popup/combo is open, send B key event to ImGui so it can close the popup
            if (g.OpenPopupStack.Size > 0) {
                ImGuiIO& io = ImGui::GetIO();
                io.AddKeyEvent(ImGuiKey_GamepadFaceRight, true);
            }
        } else {
            s_popup_was_open_on_b_press = false;
            // Release the B key when not pressed
            ImGuiIO& io = ImGui::GetIO();
            io.AddKeyEvent(ImGuiKey_GamepadFaceRight, false);
        }
        
        ImGui::NewFrame();
        return true;
    }
    
    void Render(void) {
        ImGui::Render();
        ImGuiIO &io = ImGui::GetIO(); (void)io;
        glViewport(0, 0, static_cast<int>(io.DisplaySize.x), static_cast<int>(io.DisplaySize.y));
        glClearColor(0.00f, 0.00f, 0.00f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplSwitch_RenderDrawData(ImGui::GetDrawData());
        GUI::SwapBuffers();
    }
    
    // Module-level clkrst sessions for stats overlay (need cleanup on exit)
    static ClkrstSession s_cpu_session = {0}, s_gpu_session = {0};
    static bool s_clkrst_sessions_open = false;
    
    static void CleanupStatsOverlay(void) {
        // Close clkrst sessions if they were opened
        if (s_clkrst_sessions_open) {
            clkrstCloseSession(&s_cpu_session);
            clkrstCloseSession(&s_gpu_session);
            s_clkrst_sessions_open = false;
        }
    }
    
    void Exit(App &app) {
        (void)app;  // Currently unused but available for future cleanup needs
        
        // Save ImGui settings (table column widths, etc.) to SD card
        ImGui::SaveIniSettingsToDisk(ImGui::GetIO().IniFilename);
        
        // Clean up stats overlay resources (clkrst sessions) before ImGui shutdown
        CleanupStatsOverlay();
        
        // Shutdown ImGui backend
        ImGui_ImplSwitch_Shutdown();
        
        // Destroy ImGui context to free all ImGui resources
        ImGui::DestroyContext();
        
        // Clean up EGL (includes glFinish() for GPU sync)
        ExitEGL();
    }
    
    bool IsHoldingToClose(float &progress) {
        if (!s_minus_is_held) {
            progress = 0.0f;
            return false;
        }
        
        u64 elapsed_ticks = armGetSystemTick() - s_minus_hold_start;
        float elapsed_seconds = (elapsed_ticks * 625.0f / 12.0f) / 1000000000.0f;
        progress = elapsed_seconds / HOLD_TO_CLOSE_SECONDS;
        if (progress > 1.0f) progress = 1.0f;
        return true;
    }
    
    void StartRefreshAnimation(void) {
        s_refresh_anim_start = armGetSystemTick();
        s_refresh_anim_active = true;
    }
    
    bool IsRefreshAnimating(float &progress) {
        if (!s_refresh_anim_active) {
            progress = 0.0f;
            return false;
        }
        
        u64 elapsed_ticks = armGetSystemTick() - s_refresh_anim_start;
        float elapsed_seconds = (elapsed_ticks * 625.0f / 12.0f) / 1000000000.0f;
        progress = elapsed_seconds / REFRESH_ANIM_SECONDS;
        
        if (progress >= 1.0f) {
            // Animation complete
            progress = 1.0f;
            s_refresh_anim_active = false;
            return false;  // Animation is done
        }
        
        return true;
    }
    
    void ResetUIState(App &app) {
        // Called after language change - the overlay will re-assert its z-order
        // in RenderStatsOverlay via BringWindowToDisplayFront
        Log::Debug("ResetUIState called - cfg.show_stats=%d, cfg.lang=%d\n", app.config.normal.show_stats, app.config.normal.lang);
    }
    
    void SetRightStickScrollSuppressed(bool suppress) {
        s_right_stick_scroll_suppressed = suppress;
    }
    
    void ResetImGuiSettings(FileSystemService &fs_svc) {
        // Clear in-memory ImGui settings (table column widths, window positions, etc.)
        ImGui::ClearIniSettings();
        
        // Delete the ini file from disk so it won't be reloaded
        static const char* imgui_ini_path = "/switch/NX-Shell/imgui.ini";
        fsFsDeleteFile(std::addressof(fs_svc.devices[FileSystemSDMC]), imgui_ini_path);
        
        Log::Debug("ResetImGuiSettings - cleared in-memory settings and deleted %s\n", imgui_ini_path);
    }
    
    bool WasPopupOpenOnBPress(void) {
        // Returns true if a popup was open when B was pressed this frame
        // Used to prevent double-handling B (ImGui closes popup, then we also switch tabs)
        return s_popup_was_open_on_b_press;
    }
    
    // Helper to build N/A display string by extracting label from format string
    static void DisplayStatsNA(const char* format_str, const char* na_str) {
        // Find the '%' to extract just the label portion
        const char* pct = std::strchr(format_str, '%');
        if (pct) {
            char buf[64];
            int label_len = static_cast<int>(pct - format_str);
            std::snprintf(buf, sizeof(buf), "%.*s%s", label_len, format_str, na_str);
            ImGui::TextDisabled("%s", buf);
        } else {
            ImGui::TextDisabled("%s", na_str);
        }
    }

    void RenderStatsOverlay(App &app) {
        if (!app.config.ShowStats()) {
            return;
        }
        
        ImGuiIO &io = ImGui::GetIO();
        const int lang = app.config.Lang();
        
        // FPS history buffer for 1 minute graph (store one sample per frame, ~60 fps = 3600 samples)
        static constexpr int FPS_HISTORY_SIZE = 3600;  // ~60 seconds at 60 fps
        static float s_fps_history[FPS_HISTORY_SIZE] = {0};
        static int s_fps_history_offset = 0;
        static float s_fps_min = 0.0f, s_fps_max = 60.0f;
        
        // Update FPS history
        s_fps_history[s_fps_history_offset] = io.Framerate;
        s_fps_history_offset = (s_fps_history_offset + 1) % FPS_HISTORY_SIZE;
        
        // Calculate min/max for graph scaling (only from valid samples)
        s_fps_min = 0.0f;
        s_fps_max = 65.0f;  // Fixed max for consistent scale
        
        // Position in top-right corner with padding
        const float padding = 10.0f;
        const float overlay_width = 286.0f;  // 220 * 1.3 = 286
        ImVec2 overlay_pos = ImVec2(io.DisplaySize.x - overlay_width - padding - 20.0f, padding + 20.0f);
        
        ImGui::SetNextWindowPos(overlay_pos, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.6f);
        
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | 
                                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                                 ImGuiWindowFlags_NoInputs;
        
        if (ImGui::Begin("##StatsOverlay", nullptr, flags)) {
            // Force this window to always be on top (in front of all other windows)
            ImGuiWindow* window = ImGui::GetCurrentWindow();
            if (window) {
                ImGui::BringWindowToDisplayFront(window);
            }
            
            // Resolution
            ImGui::Text(strings[lang][Lang::StatsResolution], app.gui.display_width, app.gui.display_height);
            
            // Framerate and frame time
            ImGui::Text(strings[lang][Lang::StatsFPS], io.Framerate, 1000.0f / io.Framerate);
            
            // FPS graph showing last 1 minute
            ImGui::PlotLines("##FPSGraph", s_fps_history, FPS_HISTORY_SIZE, s_fps_history_offset,
                             nullptr, s_fps_min, s_fps_max, ImVec2(overlay_width - 16.0f, 40.0f));
            
            // CPU/GPU Clock speeds (using clkrst service)
            if (!s_clkrst_sessions_open) {
                if (R_SUCCEEDED(clkrstOpenSession(&s_cpu_session, PcvModuleId_CpuBus, 3)))
                    s_clkrst_sessions_open = true;
                clkrstOpenSession(&s_gpu_session, PcvModuleId_GPU, 3);
            }
            
            if (s_clkrst_sessions_open) {
                u32 cpu_hz = 0, gpu_hz = 0;
                if (R_SUCCEEDED(clkrstGetClockRate(&s_cpu_session, &cpu_hz))) {
                    ImGui::Text(strings[lang][Lang::StatsCPU], cpu_hz / 1000000);
                }
                if (R_SUCCEEDED(clkrstGetClockRate(&s_gpu_session, &gpu_hz))) {
                    ImGui::Text(strings[lang][Lang::StatsGPU], gpu_hz / 1000000);
                }
            }
            
            // Memory usage
            u64 used_mem = 0, total_mem = 0;
            svcGetInfo(&used_mem, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);
            svcGetInfo(&total_mem, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
            
            float used_mb = used_mem / (1024.0f * 1024.0f);
            float total_mb = total_mem / (1024.0f * 1024.0f);
            ImGui::Text(strings[lang][Lang::StatsMemory], used_mb, total_mb);
            
            // Temperatures - try multiple methods for compatibility with all Switch models
            s32 soc_temp_mc = 0, skin_temp_mc = 0;
            bool got_soc = false, got_skin = false;
            
            // Try ts service first (works on Erista)
            if (R_SUCCEEDED(tsGetTemperatureMilliC(TsLocation_Internal, &soc_temp_mc))) {
                got_soc = true;
            }
            
            // Try tc service for skin temperature (more reliable on Mariko)
            if (R_SUCCEEDED(tcGetSkinTemperatureMilliC(&skin_temp_mc))) {
                got_skin = true;
            } else if (R_SUCCEEDED(tsGetTemperatureMilliC(TsLocation_External, &skin_temp_mc))) {
                // Fallback to ts service for skin/PCB temp
                got_skin = true;
            }
            
            if (got_soc) {
                float soc_temp_c = soc_temp_mc / 1000.0f;
                ImGui::Text(strings[lang][Lang::StatsSOCTemp], soc_temp_c);
            } else {
                DisplayStatsNA(strings[lang][Lang::StatsSOCTemp], strings[lang][Lang::StatsNA]);
            }
            
            if (got_skin) {
                float skin_temp_c = skin_temp_mc / 1000.0f;
                ImGui::Text(strings[lang][Lang::StatsSkinTemp], skin_temp_c);
            } else {
                DisplayStatsNA(strings[lang][Lang::StatsSkinTemp], strings[lang][Lang::StatsNA]);
            }
        }
        ImGui::End();
    }
}

namespace Toast {
    void DrawFilename(ConfigService &config_svc, const char* text) {
        ImDrawList *draw_list = ImGui::GetForegroundDrawList();
        
        ImVec2 text_size = ImGui::CalcTextSize(text);
        
        // Toast positioned at top-left with 2px offset
        const float padding_x = 10.0f;
        const float padding_y = 6.0f;
        const float toast_x = 2.0f;
        const float toast_y = 2.0f;
        const float toast_w = text_size.x + padding_x * 2;
        const float toast_h = text_size.y + padding_y * 2;
        
        // Theme-aware colors
        const bool is_dark = GUI::IsCurrentThemeDark(config_svc);
        ImU32 bg_color = is_dark ? IM_COL32(0, 0, 0, 180) : IM_COL32(255, 255, 255, 220);
        ImU32 text_color = is_dark ? IM_COL32(255, 255, 255, 255) : IM_COL32(0, 0, 0, 255);
        
        draw_list->AddRectFilled(
            ImVec2(toast_x, toast_y), 
            ImVec2(toast_x + toast_w, toast_y + toast_h), 
            bg_color, 6.0f
        );
        draw_list->AddText(
            ImVec2(toast_x + padding_x, toast_y + padding_y), 
            text_color, 
            text
        );
    }
    
    void DrawCentered(GUIService &gui_svc, const char* text, float alpha) {
        ImDrawList *draw_list = ImGui::GetForegroundDrawList();
        
        const float display_w = static_cast<float>(gui_svc.display_width);
        const float display_h = static_cast<float>(gui_svc.display_height);
        
        ImVec2 text_size = ImGui::CalcTextSize(text);
        
        // Toast background with padding, centered near bottom
        const float padding_x = 20.0f;
        const float padding_y = 10.0f;
        const float toast_w = text_size.x + padding_x * 2;
        const float toast_h = text_size.y + padding_y * 2;
        const float toast_x = (display_w - toast_w) * 0.5f;
        const float toast_y = display_h - toast_h - 60.0f;  // Position near bottom
        
        ImU32 bg_color = IM_COL32(0, 0, 0, static_cast<int>(200 * alpha));
        ImU32 text_color = IM_COL32(255, 255, 255, static_cast<int>(255 * alpha));
        
        draw_list->AddRectFilled(
            ImVec2(toast_x, toast_y), 
            ImVec2(toast_x + toast_w, toast_y + toast_h), 
            bg_color, 8.0f
        );
        draw_list->AddText(
            ImVec2(toast_x + padding_x, toast_y + padding_y), 
            text_color, 
            text
        );
    }
}