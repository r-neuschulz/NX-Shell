#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <glad/glad.h>
#include <cstdio>
#include <memory>
#include <switch.h>

#include "config.hpp"
#include "gui.hpp"
#include "imgui_impl_switch.hpp"
#include "log.hpp"

namespace GUI {
    static EGLDisplay s_display = EGL_NO_DISPLAY;
    static EGLContext s_context = EGL_NO_CONTEXT;
    static EGLSurface s_surface = EGL_NO_SURFACE;
    static EGLConfig s_config = nullptr;
    static NWindow *s_window = nullptr;
    static AppletOperationMode s_operation_mode = AppletOperationMode_Handheld;
    
    // Display dimensions - exported for use by other modules
    int display_width = 1280;
    int display_height = 720;
    
    bool IsDocked(void) {
        return s_operation_mode == AppletOperationMode_Console;
    }
    
    // Recreate the EGL surface for new dimensions (needed for runtime resolution changes)
    static bool RecreateSurface(void) {
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
        nwindowSetDimensions(s_window, display_width, display_height);
        
        // Create new surface with updated dimensions
        s_surface = eglCreateWindowSurface(s_display, s_config, s_window, nullptr);
        if (!s_surface) {
            Log::Error("Surface recreation failed! error: %d", eglGetError());
            return false;
        }
        
        // Rebind the new surface
        eglMakeCurrent(s_display, s_surface, s_surface, s_context);
        
        // Update the GL viewport to match new dimensions
        glViewport(0, 0, display_width, display_height);
        
        return true;
    }
    
    void UpdateDisplayDimensions(void) {
        AppletOperationMode mode = appletGetOperationMode();
        s_operation_mode = mode;
        
        // Determine target resolution based on config setting
        int target_width = 1280;
        int target_height = 720;
        
        switch (cfg.resolution_mode) {
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
        if (display_width != target_width || display_height != target_height) {
            display_width = target_width;
            display_height = target_height;
            
            // Recreate the EGL surface for the new resolution
            RecreateSurface();
        }
    }
    
    static bool InitEGL(NWindow* win) {
        s_window = win;
        
        // Check initial dock state and set dimensions accordingly
        s_operation_mode = appletGetOperationMode();
        if (IsDocked()) {
            display_width = 1920;
            display_height = 1080;
        } else {
            display_width = 1280;
            display_height = 720;
        }
        
        // Set native window dimensions to match current mode
        nwindowSetDimensions(win, display_width, display_height);
        
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
        return true;
    }
    
    static void ExitEGL(void) {
        if (s_display) {
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
        return eglSwapBuffers(s_display, s_surface);
    }

    void SetDefaultTheme(void) {
        ImGui::GetStyle().FrameRounding = 4.0f;
        ImGui::GetStyle().GrabRounding = 4.0f;
        
        ImVec4 *colors = ImGui::GetStyle().Colors;
        colors[ImGuiCol_Text] = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.36f, 0.42f, 0.47f, 1.00f);
        colors[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
        colors[ImGuiCol_Border] = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
        colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.12f, 0.20f, 0.28f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.09f, 0.12f, 0.14f, 1.00f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.12f, 0.14f, 0.65f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
        colors[ImGuiCol_MenuBarBg] = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.39f);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.18f, 0.22f, 0.25f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.09f, 0.21f, 0.31f, 1.00f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.00f, 0.50f, 0.50f, 1.0f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.37f, 0.61f, 1.00f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.00f, 0.50f, 0.50f, 1.0f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.00f, 0.50f, 0.50f, 1.0f);
        colors[ImGuiCol_Header] = ImVec4(0.20f, 0.25f, 0.29f, 0.55f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.00f, 0.50f, 0.50f, 1.0f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.00f, 0.50f, 0.50f, 1.0f);
        colors[ImGuiCol_Separator] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
        colors[ImGuiCol_SeparatorHovered] = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
        colors[ImGuiCol_SeparatorActive] = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
        colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.25f);
        colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
        colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
        colors[ImGuiCol_Tab] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
        colors[ImGuiCol_TabHovered] = ImVec4(0.00f, 0.50f, 0.50f, 1.0f);
        colors[ImGuiCol_TabSelected] = ImVec4(0.00f, 0.50f, 0.50f, 1.0f);
        colors[ImGuiCol_TabDimmed] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
        colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
        colors[ImGuiCol_PlotLines] = ImVec4(0.00f, 0.50f, 0.50f, 1.0f);
        colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
        colors[ImGuiCol_PlotHistogram] = ImVec4(0.00f, 0.50f, 0.50f, 1.0f);
        colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
        colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
        colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
        colors[ImGuiCol_NavCursor] = ImVec4(0.00f, 0.50f, 0.50f, 1.0f);
        colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
        colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
    }

    bool Init(void) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        (void)io;
        
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        
        if (!GUI::InitEGL(nwindowGetDefault()))
            return false;
        
        gladLoadGL();
        
        ImGui_ImplSwitch_Init("#version 130");
        
        // Load nintendo font
        PlFontData standard, extended, chinese, korean;
        static ImWchar extended_range[] = {0xE000, 0xE152, 0};
        
        // CJK glyph ranges (modern replacement for obsolete GetGlyphRanges* functions)
        // On-demand loading will handle additional glyphs as needed
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
        
        if ((R_SUCCEEDED(plGetSharedFontByType(std::addressof(standard), PlSharedFontType_Standard))) &&
            R_SUCCEEDED(plGetSharedFontByType(std::addressof(extended), PlSharedFontType_NintendoExt)) &&
            R_SUCCEEDED(plGetSharedFontByType(std::addressof(chinese), PlSharedFontType_ChineseSimplified)) &&
            R_SUCCEEDED(plGetSharedFontByType(std::addressof(korean), PlSharedFontType_KO))) {
                
            ImFontConfig font_cfg;
            
            font_cfg.FontDataOwnedByAtlas = false;
            io.Fonts->AddFontFromMemoryTTF(standard.address, standard.size, 20.f, std::addressof(font_cfg), io.Fonts->GetGlyphRangesDefault());

            if (cfg.multi_lang) {
                font_cfg.MergeMode = true;
                io.Fonts->AddFontFromMemoryTTF(extended.address, extended.size, 20.f, std::addressof(font_cfg), extended_range);
                io.Fonts->AddFontFromMemoryTTF(chinese.address,  chinese.size,  20.f, std::addressof(font_cfg), chinese_ranges);
                io.Fonts->AddFontFromMemoryTTF(korean.address,   korean.size,   20.f, std::addressof(font_cfg), korean_ranges);
            }
            
            io.Fonts->Flags |= ImFontAtlasFlags_NoPowerOfTwoHeight;
            // Font atlas is built automatically when needed by the new texture system
        }

        GUI::SetDefaultTheme();
        return true;
    }
    
    bool Loop(u64 &key) {
        if (!appletMainLoop())
            return false;
        
        // Check for dock/undock and update resolution if needed
        UpdateDisplayDimensions();
        
        key = ImGui_ImplSwitch_NewFrame();
        ImGui::NewFrame();
        return !(key & HidNpadButton_Plus);
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
    
    void Exit(void) {
        ImGui_ImplSwitch_Shutdown();
        GUI::ExitEGL();
    }
}
