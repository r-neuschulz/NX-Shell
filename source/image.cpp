#include <string>

#include "config.hpp"
#include "gui.hpp"
#include "imgui.h"
#include "imgui_impl_switch.hpp"
#include "language.hpp"
#include "popups.hpp"
#include "windows.hpp"

#include "imgui_internal.h"

// Use GUI module's display dimensions for native resolution support

namespace ImageViewer {
    // Frame counter for delayed pre-loading (don't pre-load immediately after navigation)
    static int s_frames_since_navigation = 0;
    static constexpr int PRELOAD_DELAY_FRAMES = 30;  // Wait ~0.5 seconds before pre-loading
    
    // Toast notification state for fullscreen mode
    static float s_toast_timer = 0.0f;
    static constexpr float TOAST_DURATION = 2.5f;  // Show toast for 2.5 seconds
    
    // Deferred texture deletion queue - textures are freed at the START of the next frame,
    // AFTER the previous frame's draw commands have been executed by OpenGL.
    // This prevents the "font texture flash" when a texture is deleted while ImGui's
    // command buffer still references it.
    static std::vector<Tex> s_textures_to_delete;
    
    static void ProcessDeferredDeletions(void) {
        for (auto &tex : s_textures_to_delete) {
            if (tex.id != 0)
                Textures::Free(tex);
        }
        s_textures_to_delete.clear();
    }
    
    static void FreeTextureVector(std::vector<Tex> &textures) {
        // Queue textures for deferred deletion instead of freeing immediately
        for (auto &tex : textures) {
            if (tex.id != 0)
                s_textures_to_delete.push_back(tex);
        }
        textures.clear();
    }
    
    void ClearTextures(void) {
        FreeTextureVector(data.textures);
        data.frame_count = 0;
    }
    
    void ClearPreloadedTextures(void) {
        FreeTextureVector(data.textures_prev);
        data.preload_prev_index = -1;
        
        FreeTextureVector(data.textures_next);
        data.preload_next_index = -1;
    }
    
    void CleanupDeferredDeletions(void) {
        // Force-process any remaining deferred deletions (called on app exit)
        ProcessDeferredDeletions();
    }

    // Find the index of the previous image file (cycling to the end if needed)
    int FindPrevImageIndex(int from_index) {
        // Search backwards from current position
        for (int i = from_index - 1; i >= 0; i--) {
            if (data.entries[i].type != FsDirEntryType_Dir) {
                std::string filename = data.entries[i].name;
                if (!filename.empty()) {
                    FileType ft = FS::GetFileType(filename.c_str());
                    if (ft == FileTypeImage)
                        return i;
                }
            }
        }
        
        // Wrap around: search from the end
        for (int i = static_cast<int>(data.entries.size()) - 1; i > from_index; i--) {
            if (data.entries[i].type != FsDirEntryType_Dir) {
                std::string filename = data.entries[i].name;
                if (!filename.empty()) {
                    FileType ft = FS::GetFileType(filename.c_str());
                    if (ft == FileTypeImage)
                        return i;
                }
            }
        }
        
        return -1;  // No other images found
    }

    // Find the index of the next image file (cycling to the beginning if needed)
    int FindNextImageIndex(int from_index) {
        // Search forwards from current position
        for (unsigned int i = from_index + 1; i < data.entries.size(); i++) {
            if (data.entries[i].type != FsDirEntryType_Dir) {
                std::string filename = data.entries[i].name;
                if (!filename.empty()) {
                    FileType ft = FS::GetFileType(filename.c_str());
                    if (ft == FileTypeImage)
                        return static_cast<int>(i);
                }
            }
        }
        
        // Wrap around: search from the beginning
        for (int i = 0; i < from_index; i++) {
            if (data.entries[i].type != FsDirEntryType_Dir) {
                std::string filename = data.entries[i].name;
                if (!filename.empty()) {
                    FileType ft = FS::GetFileType(filename.c_str());
                    if (ft == FileTypeImage)
                        return i;
                }
            }
        }
        
        return -1;  // No other images found
    }

    // Pre-load ONE adjacent image (called only when idle, alternates between prev/next)
    void PreloadAdjacentImages(void) {
        // Don't pre-load immediately after navigation - wait for user to settle
        if (s_frames_since_navigation < PRELOAD_DELAY_FRAMES) {
            s_frames_since_navigation++;
            return;
        }
        
        int current = static_cast<int>(data.selected);
        
        // Find prev and next image indices
        int prev_idx = FindPrevImageIndex(current);
        int next_idx = FindNextImageIndex(current);
        
        // Check what needs pre-loading
        bool need_prev = (prev_idx >= 0 && prev_idx != data.preload_prev_index && data.textures_prev.empty());
        bool need_next = (next_idx >= 0 && next_idx != data.preload_next_index && data.textures_next.empty());
        
        // Only load ONE image per call to avoid blocking too long
        // Prioritize the direction that doesn't have a pre-load yet
        if (need_next) {
            char fs_path[FS_MAX_PATH + 1];
            if (std::snprintf(fs_path, FS_MAX_PATH, "%s/%s", cwd.c_str(), data.entries[next_idx].name) > 0) {
                if (Textures::LoadImageFile(fs_path, data.textures_next)) {
                    data.preload_next_index = next_idx;
                } else {
                    data.preload_next_index = -1;
                }
            }
        }
        else if (need_prev) {
            char fs_path[FS_MAX_PATH + 1];
            if (std::snprintf(fs_path, FS_MAX_PATH, "%s/%s", cwd.c_str(), data.entries[prev_idx].name) > 0) {
                if (Textures::LoadImageFile(fs_path, data.textures_prev)) {
                    data.preload_prev_index = prev_idx;
                } else {
                    data.preload_prev_index = -1;
                }
            }
        }
    }

    bool HandleScroll(int index) {
        if (data.entries[index].type == FsDirEntryType_Dir)
            return false;
        
        // Load into temporary first
        std::vector<Tex> new_textures;
        char fs_path[FS_MAX_PATH + 1];
        if (std::snprintf(fs_path, FS_MAX_PATH, "%s/%s", cwd.c_str(), data.entries[index].name) > 0) {
            if (Textures::LoadImageFile(fs_path, new_textures)) {
                // Success - now do atomic swap
                FreeTextureVector(data.textures);
                data.textures = std::move(new_textures);
                data.selected = index;
                data.frame_count = 0;
                return true;
            }
        }
        return false;
    }

    bool HandlePrev(void) {
        int prev_idx = FindPrevImageIndex(static_cast<int>(data.selected));
        
        if (prev_idx < 0)
            return false;  // No other images, stay on current
        
        // Reset pre-load delay counter
        s_frames_since_navigation = 0;
        
        // Check if we have this image pre-loaded and ready
        if (prev_idx == data.preload_prev_index && !data.textures_prev.empty() && data.textures_prev[0].id != 0) {
            // Hand over current image to become next preload (avoid re-decoding)
            // Free the old next preload first (it's now 2 images ahead, stale)
            FreeTextureVector(data.textures_next);
            
            // Move current -> next preload (the image we just left is now "next")
            data.textures_next = std::move(data.textures);
            data.preload_next_index = static_cast<int>(data.selected);
            
            // Move prev preload -> current
            data.textures = std::move(data.textures_prev);
            data.textures_prev.clear();
            data.preload_prev_index = -1;
            
            data.selected = prev_idx;
            data.frame_count = 0;
            return true;
        }
        
        // Pre-load not available - load directly (keeps current image visible during load)
        std::vector<Tex> new_textures;
        char fs_path[FS_MAX_PATH + 1];
        if (std::snprintf(fs_path, FS_MAX_PATH, "%s/%s", cwd.c_str(), data.entries[prev_idx].name) > 0) {
            if (Textures::LoadImageFile(fs_path, new_textures)) {
                // Hand over current image to become next preload (avoid re-decoding)
                // Free the old next preload first
                FreeTextureVector(data.textures_next);
                
                // Move current -> next preload
                data.textures_next = std::move(data.textures);
                data.preload_next_index = static_cast<int>(data.selected);
                
                // Set new current
                data.textures = std::move(new_textures);
                
                // Clear prev preload (will be loaded by PreloadAdjacentImages)
                FreeTextureVector(data.textures_prev);
                data.preload_prev_index = -1;
                
                data.selected = prev_idx;
                data.frame_count = 0;
                return true;
            }
        }
        
        return false;
    }

    bool HandleNext(void) {
        int next_idx = FindNextImageIndex(static_cast<int>(data.selected));
        
        if (next_idx < 0)
            return false;  // No other images, stay on current
        
        // Reset pre-load delay counter
        s_frames_since_navigation = 0;
        
        // Check if we have this image pre-loaded and ready
        if (next_idx == data.preload_next_index && !data.textures_next.empty() && data.textures_next[0].id != 0) {
            // Hand over current image to become prev preload (avoid re-decoding)
            // Free the old prev preload first (it's now 2 images behind, stale)
            FreeTextureVector(data.textures_prev);
            
            // Move current -> prev preload (the image we just left is now "prev")
            data.textures_prev = std::move(data.textures);
            data.preload_prev_index = static_cast<int>(data.selected);
            
            // Move next preload -> current
            data.textures = std::move(data.textures_next);
            data.textures_next.clear();
            data.preload_next_index = -1;
            
            data.selected = next_idx;
            data.frame_count = 0;
            return true;
        }
        
        // Pre-load not available - load directly (keeps current image visible during load)
        std::vector<Tex> new_textures;
        char fs_path[FS_MAX_PATH + 1];
        if (std::snprintf(fs_path, FS_MAX_PATH, "%s/%s", cwd.c_str(), data.entries[next_idx].name) > 0) {
            if (Textures::LoadImageFile(fs_path, new_textures)) {
                // Hand over current image to become prev preload (avoid re-decoding)
                // Free the old prev preload first
                FreeTextureVector(data.textures_prev);
                
                // Move current -> prev preload
                data.textures_prev = std::move(data.textures);
                data.preload_prev_index = static_cast<int>(data.selected);
                
                // Set new current
                data.textures = std::move(new_textures);
                
                // Clear next preload (will be loaded by PreloadAdjacentImages)
                FreeTextureVector(data.textures_next);
                data.preload_next_index = -1;
                
                data.selected = next_idx;
                data.frame_count = 0;
                return true;
            }
        }
        
        return false;
    }

    void HandleControls(u64 &key, bool &properties) {
        if (key & HidNpadButton_X)
            properties = true;
        
        // D-pad zoom controls
        if (ImGui::IsKeyDown(ImGuiKey_GamepadDpadDown)) {
            data.zoom_factor -= 0.5f * ImGui::GetIO().DeltaTime;
            
            if (data.zoom_factor < 0.1f)
                data.zoom_factor = 0.1f;
        }
        else if (ImGui::IsKeyDown(ImGuiKey_GamepadDpadUp)) {
            data.zoom_factor += 0.5f * ImGui::GetIO().DeltaTime;
            
            if (data.zoom_factor > 5.0f)
                data.zoom_factor = 5.0f;
        }
        
        // Touch drag zoom - vertical drag changes zoom level
        float touch_delta_x = 0.f, touch_delta_y = 0.f;
        if (ImGui_ImplSwitch_GetTouchState(&touch_delta_x, &touch_delta_y, nullptr, nullptr)) {
            // Vertical drag: drag up (negative delta) = zoom in, drag down (positive delta) = zoom out
            // Scale factor: ~300 pixels of drag for full zoom range
            const float zoom_sensitivity = 0.004f;
            data.zoom_factor -= touch_delta_y * zoom_sensitivity;
            
            // Clamp zoom factor
            if (data.zoom_factor < 0.1f)
                data.zoom_factor = 0.1f;
            else if (data.zoom_factor > 5.0f)
                data.zoom_factor = 5.0f;
        }
        
        // ZR toggles fullscreen mode
        if (key & HidNpadButton_ZR) {
            data.image_fullscreen = !data.image_fullscreen;
            if (data.image_fullscreen) {
                s_toast_timer = TOAST_DURATION;  // Show toast when entering fullscreen
            }
        }
        
        if (!properties) {
            if (key & HidNpadButton_B) {
                ImageViewer::ClearTextures();
                ImageViewer::ClearPreloadedTextures();
                // Don't process deferred deletions here - this frame's draw commands
                // still reference the textures. They'll be freed at the start of
                // the next frame by CleanupDeferredDeletions() in MainWindow.
                data.zoom_factor = 1.0f;
                data.image_fullscreen = false;
                s_frames_since_navigation = 0;
                data.state = WINDOW_STATE_FILEBROWSER;
            }
            
            if (key & HidNpadButton_L) {
                ImageViewer::HandlePrev();
            }
            else if (key & HidNpadButton_R) {
                ImageViewer::HandleNext();
            }
        }
    }
}

namespace Windows {
    static void DrawImageViewerBottomBar(void) {
        const int lang = Config::GetLang();
        ImDrawList *draw_list = ImGui::GetForegroundDrawList();
        
        // Bottom bar dimensions
        const float button_bar_height = 45.0f;
        const float button_radius = 12.0f;
        const float hint_spacing = 20.0f;
        const float display_w = static_cast<float>(GUI::display_width);
        const float display_h = static_cast<float>(GUI::display_height);
        
        // Bar position at bottom of screen
        const float bar_y = display_h - button_bar_height;
        const float center_y = bar_y + button_bar_height * 0.5f;
        
        // Theme-aware colors
        const bool is_dark = GUI::IsCurrentThemeDark();
        
        // Draw semi-transparent background bar (dark or light based on theme)
        const ImU32 bar_bg = is_dark ? IM_COL32(0, 0, 0, 180) : IM_COL32(240, 240, 245, 220);
        draw_list->AddRectFilled(ImVec2(0, bar_y), ImVec2(display_w, display_h), bar_bg);
        
        // Button colors - use style-aware helpers from GUI module
        const ImU32 color_b = GUI::GetButtonColorB();
        const ImU32 color_x = GUI::GetButtonColorX();
        const ImU32 color_text = GUI::GetButtonTextColor();
        const ImU32 color_label = GUI::GetThemeLabelColor();
        const ImU32 color_minus_bg = GUI::GetButtonColorMinus();
        
        // Shoulder button style dimensions (matching file browser ZR button)
        const float shoulder_width = 32.0f;
        const float shoulder_height = 18.0f;
        const float shoulder_chamfer = 6.0f;
        const ImU32 color_shoulder = is_dark ? IM_COL32(100, 100, 100, 255) : IM_COL32(150, 150, 155, 255);
        
        // DPad style (for zoom hints)
        const float dpad_size = 10.0f;
        const ImU32 color_dpad = is_dark ? IM_COL32(80, 80, 80, 255) : IM_COL32(140, 140, 145, 255);
        
        // ZR button dimensions
        const float zr_width = 32.0f;
        const float zr_height = 18.0f;
        const float zr_chamfer = 6.0f;
        
        // Left side: (-) Quit with hold-to-close indicator
        {
            float left_x = 20.0f;
            ImVec2 minus_center(left_x + button_radius, center_y);
            
            // Check if we're holding to close
            float hold_progress = 0.0f;
            bool is_holding = GUI::IsHoldingToClose(hold_progress);
            
            // Draw background circle
            draw_list->AddCircleFilled(minus_center, button_radius, color_minus_bg, 24);
            
            // Draw progress arc when holding
            if (is_holding && hold_progress > 0.0f) {
                const float arc_radius = button_radius + 3.0f;
                const float start_angle = -IM_PI * 0.5f;
                const float end_angle = start_angle + (hold_progress * IM_PI * 2.0f);
                
                const int num_segments = static_cast<int>(24 * hold_progress) + 1;
                for (int i = 0; i < num_segments; i++) {
                    float a1 = start_angle + (end_angle - start_angle) * (static_cast<float>(i) / num_segments);
                    float a2 = start_angle + (end_angle - start_angle) * (static_cast<float>(i + 1) / num_segments);
                    ImVec2 p1(minus_center.x + cosf(a1) * arc_radius, minus_center.y + sinf(a1) * arc_radius);
                    ImVec2 p2(minus_center.x + cosf(a2) * arc_radius, minus_center.y + sinf(a2) * arc_radius);
                    draw_list->AddLine(p1, p2, GUI::GetAccentColorU32(), 3.0f);
                }
            }
            
            // Draw minus sign
            const float minus_width = button_radius * 0.8f;
            draw_list->AddLine(
                ImVec2(minus_center.x - minus_width, minus_center.y),
                ImVec2(minus_center.x + minus_width, minus_center.y),
                color_text, 2.0f
            );
            
            // Draw label
            float label_x = left_x + button_radius * 2 + 6.0f;
            ImU32 label_color = is_holding 
                ? GUI::GetAccentColorU32WithAlpha(200 + static_cast<int>(55 * hold_progress))
                : color_label;
            draw_list->AddText(ImVec2(label_x, center_y - ImGui::GetTextLineHeight() * 0.5f), label_color, strings[lang][Lang::HintExit]);
        }
        
        // Calculate total width of right-aligned hints
        float total_width = 0.0f;
        
        total_width += button_radius * 2 + 6.0f + ImGui::CalcTextSize(strings[lang][Lang::HintBack]).x + hint_spacing;
        total_width += button_radius * 2 + 6.0f + ImGui::CalcTextSize(strings[lang][Lang::HintProperties]).x + hint_spacing;
        total_width += shoulder_width + 6.0f + ImGui::CalcTextSize(strings[lang][Lang::HintPrev]).x + hint_spacing;
        total_width += shoulder_width + 6.0f + ImGui::CalcTextSize(strings[lang][Lang::HintNext]).x + hint_spacing;
        total_width += dpad_size * 2 + 6.0f + ImGui::CalcTextSize(strings[lang][Lang::HintZoomIn]).x + hint_spacing;
        total_width += dpad_size * 2 + 6.0f + ImGui::CalcTextSize(strings[lang][Lang::HintZoomOut]).x + hint_spacing;
        total_width += zr_width + 6.0f + ImGui::CalcTextSize(strings[lang][Lang::HintFullscreen]).x;
        
        float x_offset = display_w - total_width - 20.0f;
        
        // Draw B button (Back)
        {
            ImVec2 center(x_offset + button_radius, center_y);
            draw_list->AddCircleFilled(center, button_radius, color_b, 24);
            ImVec2 text_size = ImGui::CalcTextSize("B");
            draw_list->AddText(ImVec2(center.x - text_size.x * 0.5f + 1.0f, center.y - text_size.y * 0.5f), color_text, "B");
            float label_x = x_offset + button_radius * 2 + 6.0f;
            draw_list->AddText(ImVec2(label_x, center_y - ImGui::GetTextLineHeight() * 0.5f), color_label, strings[lang][Lang::HintBack]);
            x_offset = label_x + ImGui::CalcTextSize(strings[lang][Lang::HintBack]).x + hint_spacing;
        }
        
        // Draw X button (Properties)
        {
            ImVec2 center(x_offset + button_radius, center_y);
            draw_list->AddCircleFilled(center, button_radius, color_x, 24);
            ImVec2 text_size = ImGui::CalcTextSize("X");
            draw_list->AddText(ImVec2(center.x - text_size.x * 0.5f + 1.0f, center.y - text_size.y * 0.5f), color_text, "X");
            float label_x = x_offset + button_radius * 2 + 6.0f;
            draw_list->AddText(ImVec2(label_x, center_y - ImGui::GetTextLineHeight() * 0.5f), color_label, strings[lang][Lang::HintProperties]);
            x_offset = label_x + ImGui::CalcTextSize(strings[lang][Lang::HintProperties]).x + hint_spacing;
        }
        
        // Draw L button (Prev)
        {
            float btn_y = center_y - shoulder_height * 0.5f;
            ImVec2 pts_l[5] = {
                ImVec2(x_offset + shoulder_chamfer, btn_y),
                ImVec2(x_offset + shoulder_width, btn_y),
                ImVec2(x_offset + shoulder_width, btn_y + shoulder_height),
                ImVec2(x_offset, btn_y + shoulder_height),
                ImVec2(x_offset, btn_y + shoulder_chamfer)
            };
            draw_list->AddConvexPolyFilled(pts_l, 5, color_shoulder);
            
            ImVec2 text_size = ImGui::CalcTextSize("L");
            draw_list->AddText(ImVec2(x_offset + shoulder_width * 0.5f - text_size.x * 0.5f, center_y - text_size.y * 0.5f), color_text, "L");
            float label_x = x_offset + shoulder_width + 6.0f;
            draw_list->AddText(ImVec2(label_x, center_y - ImGui::GetTextLineHeight() * 0.5f), color_label, strings[lang][Lang::HintPrev]);
            x_offset = label_x + ImGui::CalcTextSize(strings[lang][Lang::HintPrev]).x + hint_spacing;
        }
        
        // Draw R button (Next)
        {
            float btn_y = center_y - shoulder_height * 0.5f;
            ImVec2 pts_r[5] = {
                ImVec2(x_offset, btn_y),
                ImVec2(x_offset + shoulder_width - shoulder_chamfer, btn_y),
                ImVec2(x_offset + shoulder_width, btn_y + shoulder_chamfer),
                ImVec2(x_offset + shoulder_width, btn_y + shoulder_height),
                ImVec2(x_offset, btn_y + shoulder_height)
            };
            draw_list->AddConvexPolyFilled(pts_r, 5, color_shoulder);
            
            ImVec2 text_size = ImGui::CalcTextSize("R");
            draw_list->AddText(ImVec2(x_offset + shoulder_width * 0.5f - text_size.x * 0.5f, center_y - text_size.y * 0.5f), color_text, "R");
            float label_x = x_offset + shoulder_width + 6.0f;
            draw_list->AddText(ImVec2(label_x, center_y - ImGui::GetTextLineHeight() * 0.5f), color_label, strings[lang][Lang::HintNext]);
            x_offset = label_x + ImGui::CalcTextSize(strings[lang][Lang::HintNext]).x + hint_spacing;
        }
        
        // Draw DPad Up (Zoom In)
        {
            float arrow_x = x_offset + dpad_size;
            ImVec2 p1(arrow_x, center_y - dpad_size);
            ImVec2 p2(arrow_x - dpad_size, center_y + dpad_size * 0.5f);
            ImVec2 p3(arrow_x + dpad_size, center_y + dpad_size * 0.5f);
            draw_list->AddTriangleFilled(p1, p2, p3, color_dpad);
            
            float label_x = x_offset + dpad_size * 2 + 6.0f;
            draw_list->AddText(ImVec2(label_x, center_y - ImGui::GetTextLineHeight() * 0.5f), color_label, strings[lang][Lang::HintZoomIn]);
            x_offset = label_x + ImGui::CalcTextSize(strings[lang][Lang::HintZoomIn]).x + hint_spacing;
        }
        
        // Draw DPad Down (Zoom Out)
        {
            float arrow_x = x_offset + dpad_size;
            ImVec2 p1(arrow_x, center_y + dpad_size);
            ImVec2 p2(arrow_x - dpad_size, center_y - dpad_size * 0.5f);
            ImVec2 p3(arrow_x + dpad_size, center_y - dpad_size * 0.5f);
            draw_list->AddTriangleFilled(p1, p2, p3, color_dpad);
            
            float label_x = x_offset + dpad_size * 2 + 6.0f;
            draw_list->AddText(ImVec2(label_x, center_y - ImGui::GetTextLineHeight() * 0.5f), color_label, strings[lang][Lang::HintZoomOut]);
            x_offset = label_x + ImGui::CalcTextSize(strings[lang][Lang::HintZoomOut]).x + hint_spacing;
        }
        
        // Draw ZR button (Fullscreen)
        {
            float zr_x = x_offset;
            float zr_y = center_y - zr_height * 0.5f;
            
            ImU32 color_zr_outline = data.image_fullscreen ? GUI::GetAccentColorU32() : (is_dark ? IM_COL32(120, 120, 120, 255) : IM_COL32(100, 100, 105, 255));
            
            ImVec2 points[5];
            points[0] = ImVec2(zr_x, zr_y);
            points[1] = ImVec2(zr_x + zr_width - zr_chamfer, zr_y);
            points[2] = ImVec2(zr_x + zr_width, zr_y + zr_chamfer);
            points[3] = ImVec2(zr_x + zr_width, zr_y + zr_height);
            points[4] = ImVec2(zr_x, zr_y + zr_height);
            
            draw_list->AddPolyline(points, 5, color_zr_outline, ImDrawFlags_Closed, 2.0f);
            
            const char* zr_text = "ZR";
            const float zr_font_size = ImGui::GetFontSize() * 0.75f;
            ImVec2 zr_text_size = ImGui::GetFont()->CalcTextSizeA(zr_font_size, FLT_MAX, 0.0f, zr_text);
            ImVec2 zr_text_pos(zr_x + (zr_width - zr_text_size.x) * 0.5f, center_y - zr_text_size.y * 0.5f);
            draw_list->AddText(ImGui::GetFont(), zr_font_size, zr_text_pos, color_zr_outline, zr_text);
            
            float zr_label_x = zr_x + zr_width + 6.0f;
            draw_list->AddText(ImVec2(zr_label_x, center_y - ImGui::GetTextLineHeight() * 0.5f), color_label, strings[lang][Lang::HintFullscreen]);
        }
    }
    
    void ImageViewer(bool &properties, bool &file_stat) {
        // Note: Deferred texture deletions are processed at the start of MainWindow
        // in window.cpp, which ensures they run every frame regardless of state.
        
        const float display_w = static_cast<float>(GUI::display_width);
        const float display_h = static_cast<float>(GUI::display_height);
        
        // Bottom bar height (must match DrawImageViewerBottomBar)
        const float bottom_bar_height = 45.0f;
        
        // In non-fullscreen mode, reduce window height to not overlap with bottom bar
        const float window_h = data.image_fullscreen ? display_h : (display_h - bottom_bar_height);
        
        // Set up window with appropriate height
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(display_w, window_h), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        
        // Remove window border in fullscreen mode
        if (data.image_fullscreen) {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        }
        
        // Always use NoTitleBar - filename is shown as toast overlay when enabled
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;
        if (data.image_fullscreen) {
            window_flags |= ImGuiWindowFlags_NoScrollbar;
        } else {
            window_flags |= ImGuiWindowFlags_HorizontalScrollbar;
        }
        
        if (ImGui::Begin(data.entries[data.selected].name, nullptr, window_flags)) {
            // Only render if we have valid textures
            if (!data.textures.empty() && data.textures[0].id != 0) {
                // Calculate zoom factor: in fullscreen mode, fit to screen
                float effective_zoom = data.zoom_factor;
                if (data.image_fullscreen) {
                    // Calculate scale to fit image within display
                    float scale_x = display_w / static_cast<float>(data.textures[0].width);
                    float scale_y = display_h / static_cast<float>(data.textures[0].height);
                    effective_zoom = (scale_x < scale_y) ? scale_x : scale_y;
                    // Don't upscale beyond 1:1 unless already zoomed
                    if (effective_zoom > 1.0f) effective_zoom = 1.0f;
                }
                
                float img_w = data.textures[0].width * effective_zoom;
                float img_h = data.textures[0].height * effective_zoom;
                
                if ((img_w <= display_w) && (img_h <= window_h))
                    ImGui::SetCursorPos((ImGui::GetWindowSize() - ImVec2(img_w, img_h)) * 0.5f);
                    
                if (data.textures.size() > 1) {
                    svcSleepThread(data.textures[data.frame_count].delay);
                    float frame_w = data.textures[data.frame_count].width * effective_zoom;
                    float frame_h = data.textures[data.frame_count].height * effective_zoom;
                    ImGui::Image(static_cast<ImTextureID>(data.textures[data.frame_count].id), ImVec2(frame_w, frame_h));
                    data.frame_count++;
                    
                    if (data.frame_count == data.textures.size() - 1)
                        data.frame_count = 0;
                }
                else
                    ImGui::Image(static_cast<ImTextureID>(data.textures[0].id), ImVec2(img_w, img_h));
            }
        }
        
        if (!data.image_fullscreen) {
            Windows::DrawImageViewerBottomBar();
        }
        
        // Draw filename toast overlay (when enabled in settings)
        if (cfg.image_filename) {
            ImDrawList *draw_list = ImGui::GetForegroundDrawList();
            
            const char* filename = data.entries[data.selected].name;
            ImVec2 text_size = ImGui::CalcTextSize(filename);
            
            // Toast positioned at top-left with 2px offset
            const float padding_x = 10.0f;
            const float padding_y = 6.0f;
            const float toast_x = 2.0f;
            const float toast_y = 2.0f;
            const float toast_w = text_size.x + padding_x * 2;
            const float toast_h = text_size.y + padding_y * 2;
            
            // Theme-aware colors
            const bool is_dark = GUI::IsCurrentThemeDark();
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
                filename
            );
        }
        
        // Draw fullscreen toast notification (temporary, shows when entering fullscreen)
        if (data.image_fullscreen && ImageViewer::s_toast_timer > 0.0f) {
            const int lang = Config::GetLang();
            ImDrawList *draw_list = ImGui::GetForegroundDrawList();
            
            // Fade out in the last 0.5 seconds
            float alpha = 1.0f;
            if (ImageViewer::s_toast_timer < 0.5f) {
                alpha = ImageViewer::s_toast_timer / 0.5f;
            }
            
            const char* toast_text = strings[lang][Lang::HintExitFullscreen];
            ImVec2 text_size = ImGui::CalcTextSize(toast_text);
            
            // Toast background with padding
            float padding_x = 20.0f;
            float padding_y = 10.0f;
            float toast_w = text_size.x + padding_x * 2;
            float toast_h = text_size.y + padding_y * 2;
            float toast_x = (display_w - toast_w) * 0.5f;
            float toast_y = display_h - toast_h - 60.0f;  // Position near bottom
            
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
                toast_text
            );
            
            // Decrement timer
            ImageViewer::s_toast_timer -= ImGui::GetIO().DeltaTime;
        }
        
        // Pre-load adjacent images only after settling delay
        ImageViewer::PreloadAdjacentImages();

        if (properties && !data.textures.empty() && data.textures[0].id != 0)
            Popups::ImageProperties(properties, data.textures[0], file_stat);
        
        ImGui::End();
        ImGui::PopStyleVar(data.image_fullscreen ? 3 : 2);  // Pop WindowBorderSize (fullscreen only), WindowPadding, WindowRounding
    }
}
