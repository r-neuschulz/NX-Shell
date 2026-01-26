#include <string>

#include "bottombar.hpp"
#include "config.hpp"
#include "services.hpp"
#include "fs.hpp"
#include "gui.hpp"
#include "imgui.h"
#include "imgui_impl_switch.hpp"
#include "language.hpp"
#include "popups.hpp"
#include "textures.hpp"
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
    
    void ClearTextures(App &app) {
        FreeTextureVector(app.window.textures);
        app.window.frame_count = 0;
    }
    
    void ClearPreloadedTextures(App &app) {
        FreeTextureVector(app.window.textures_prev);
        app.window.preload_prev_index = -1;
        
        FreeTextureVector(app.window.textures_next);
        app.window.preload_next_index = -1;
    }
    
    void CleanupDeferredDeletions(void) {
        // Force-process any remaining deferred deletions (called on app exit)
        ProcessDeferredDeletions();
    }

    // Helper to check if an entry is a valid image file
    static bool IsImageEntry(App &app, int index) {
        if (index < 0 || index >= static_cast<int>(app.window.entries.size()))
            return false;
        if (app.window.entries[index].type == FsDirEntryType_Dir)
            return false;
        const char* name = app.window.entries[index].name;
        return name[0] != '\0' && FS::GetFileType(name) == FileTypeImage;
    }

    // Find adjacent image file in given direction (direction: -1 = prev, +1 = next)
    static int FindAdjacentImageIndex(App &app, int from_index, int direction) {
        const int count = static_cast<int>(app.window.entries.size());
        
        // Search in direction from current position
        for (int i = from_index + direction; direction > 0 ? i < count : i >= 0; i += direction) {
            if (IsImageEntry(app, i))
                return i;
        }
        
        // Wrap around: search from opposite end
        int wrap_start = direction > 0 ? 0 : count - 1;
        int wrap_end = from_index;
        for (int i = wrap_start; direction > 0 ? i < wrap_end : i > wrap_end; i += direction) {
            if (IsImageEntry(app, i))
                return i;
        }
        
        return -1;  // No other images found
    }

    // Public API wrappers
    int FindPrevImageIndex(App &app, int from_index) { return FindAdjacentImageIndex(app, from_index, -1); }
    int FindNextImageIndex(App &app, int from_index) { return FindAdjacentImageIndex(app, from_index, +1); }

    // Pre-load ONE adjacent image (called only when idle, alternates between prev/next)
    void PreloadAdjacentImages(App &app) {
        // Don't pre-load immediately after navigation - wait for user to settle
        if (s_frames_since_navigation < PRELOAD_DELAY_FRAMES) {
            s_frames_since_navigation++;
            return;
        }
        
        int current = static_cast<int>(app.window.selected);
        
        // Find prev and next image indices
        int prev_idx = FindPrevImageIndex(app, current);
        int next_idx = FindNextImageIndex(app, current);
        
        // Check what needs pre-loading
        bool need_prev = (prev_idx >= 0 && prev_idx != app.window.preload_prev_index && app.window.textures_prev.empty());
        bool need_next = (next_idx >= 0 && next_idx != app.window.preload_next_index && app.window.textures_next.empty());
        
        // Only load ONE image per call to avoid blocking too long
        // Prioritize the direction that doesn't have a pre-load yet
        if (need_next) {
            char fs_path[FS_MAX_PATH + 1];
            if (std::snprintf(fs_path, FS_MAX_PATH, "%s/%s", app.fs.cwd.c_str(), app.window.entries[next_idx].name) > 0) {
                if (Textures::LoadImageFile(fs_path, app.window.textures_next)) {
                    app.window.preload_next_index = next_idx;
                } else {
                    app.window.preload_next_index = -1;
                }
            }
        }
        else if (need_prev) {
            char fs_path[FS_MAX_PATH + 1];
            if (std::snprintf(fs_path, FS_MAX_PATH, "%s/%s", app.fs.cwd.c_str(), app.window.entries[prev_idx].name) > 0) {
                if (Textures::LoadImageFile(fs_path, app.window.textures_prev)) {
                    app.window.preload_prev_index = prev_idx;
                } else {
                    app.window.preload_prev_index = -1;
                }
            }
        }
    }

    bool HandleScroll(App &app, int index) {
        if (app.window.entries[index].type == FsDirEntryType_Dir)
            return false;
        
        // Load into temporary first
        std::vector<Tex> new_textures;
        char fs_path[FS_MAX_PATH + 1];
        if (std::snprintf(fs_path, FS_MAX_PATH, "%s/%s", app.fs.cwd.c_str(), app.window.entries[index].name) > 0) {
            if (Textures::LoadImageFile(fs_path, new_textures)) {
                // Success - now do atomic swap
                FreeTextureVector(app.window.textures);
                app.window.textures = std::move(new_textures);
                app.window.selected = index;
                app.window.frame_count = 0;
                return true;
            }
        }
        return false;
    }

    // Navigate to adjacent image (direction: -1 = prev, +1 = next)
    static bool HandleNavigate(App &app, int direction) {
        int target_idx = FindAdjacentImageIndex(app, static_cast<int>(app.window.selected), direction);
        
        if (target_idx < 0)
            return false;  // No other images, stay on current
        
        // Reset pre-load delay counter
        s_frames_since_navigation = 0;
        
        // Select source/opposite preload buffers based on direction
        // When going prev (-1): source = textures_prev, opposite = textures_next
        // When going next (+1): source = textures_next, opposite = textures_prev
        auto& source_textures = direction < 0 ? app.window.textures_prev : app.window.textures_next;
        auto& opposite_textures = direction < 0 ? app.window.textures_next : app.window.textures_prev;
        int& source_preload_index = direction < 0 ? app.window.preload_prev_index : app.window.preload_next_index;
        int& opposite_preload_index = direction < 0 ? app.window.preload_next_index : app.window.preload_prev_index;
        
        // Check if we have this image pre-loaded and ready
        if (target_idx == source_preload_index && !source_textures.empty() && source_textures[0].id != 0) {
            // Hand over current image to become opposite preload (avoid re-decoding)
            // Free the old opposite preload first (it's now 2 images away, stale)
            FreeTextureVector(opposite_textures);
            
            // Move current -> opposite preload (the image we just left)
            opposite_textures = std::move(app.window.textures);
            opposite_preload_index = static_cast<int>(app.window.selected);
            
            // Move source preload -> current
            app.window.textures = std::move(source_textures);
            source_textures.clear();
            source_preload_index = -1;
            
            app.window.selected = target_idx;
            app.window.frame_count = 0;
            app.window.pan_offset_x = 0.0f;
            app.window.pan_offset_y = 0.0f;
            return true;
        }
        
        // Pre-load not available - load directly (keeps current image visible during load)
        std::vector<Tex> new_textures;
        char fs_path[FS_MAX_PATH + 1];
        if (std::snprintf(fs_path, FS_MAX_PATH, "%s/%s", app.fs.cwd.c_str(), app.window.entries[target_idx].name) > 0) {
            if (Textures::LoadImageFile(fs_path, new_textures)) {
                // Hand over current image to become opposite preload (avoid re-decoding)
                FreeTextureVector(opposite_textures);
                opposite_textures = std::move(app.window.textures);
                opposite_preload_index = static_cast<int>(app.window.selected);
                
                // Set new current
                app.window.textures = std::move(new_textures);
                
                // Clear source preload (will be loaded by PreloadAdjacentImages)
                FreeTextureVector(source_textures);
                source_preload_index = -1;
                
                app.window.selected = target_idx;
                app.window.frame_count = 0;
                app.window.pan_offset_x = 0.0f;
                app.window.pan_offset_y = 0.0f;
                return true;
            }
        }
        
        return false;
    }

    // Public API wrappers
    bool HandlePrev(App &app) { return HandleNavigate(app, -1); }
    bool HandleNext(App &app) { return HandleNavigate(app, +1); }

    void HandleControls(App &app, u64 &key, bool &properties) {
        if (key & HidNpadButton_X)
            properties = true;
        
        // D-pad zoom controls
        if (ImGui::IsKeyDown(ImGuiKey_GamepadDpadDown)) {
            app.window.zoom_factor -= 0.5f * ImGui::GetIO().DeltaTime;
            
            if (app.window.zoom_factor < 0.1f)
                app.window.zoom_factor = 0.1f;
        }
        else if (ImGui::IsKeyDown(ImGuiKey_GamepadDpadUp)) {
            app.window.zoom_factor += 0.5f * ImGui::GetIO().DeltaTime;
            
            if (app.window.zoom_factor > 5.0f)
                app.window.zoom_factor = 5.0f;
        }
        
        // Touch drag zoom - vertical drag changes zoom level
        float touch_delta_x = 0.f, touch_delta_y = 0.f;
        if (ImGui_ImplSwitch_GetTouchState(&touch_delta_x, &touch_delta_y, nullptr, nullptr)) {
            // Vertical drag: drag up (negative delta) = zoom in, drag down (positive delta) = zoom out
            // Scale factor: ~300 pixels of drag for full zoom range
            const float zoom_sensitivity = 0.004f;
            app.window.zoom_factor -= touch_delta_y * zoom_sensitivity;
            
            // Clamp zoom factor
            if (app.window.zoom_factor < 0.1f)
                app.window.zoom_factor = 0.1f;
            else if (app.window.zoom_factor > 5.0f)
                app.window.zoom_factor = 5.0f;
        }
        
        // R stick panning - navigate around the zoomed image
        const float pan_speed = 500.0f * ImGui::GetIO().DeltaTime;
        if (ImGui::IsKeyDown(ImGuiKey_GamepadRStickUp))
            app.window.pan_offset_y -= pan_speed;
        if (ImGui::IsKeyDown(ImGuiKey_GamepadRStickDown))
            app.window.pan_offset_y += pan_speed;
        if (ImGui::IsKeyDown(ImGuiKey_GamepadRStickLeft))
            app.window.pan_offset_x -= pan_speed;
        if (ImGui::IsKeyDown(ImGuiKey_GamepadRStickRight))
            app.window.pan_offset_x += pan_speed;
        
        // ZR toggles fullscreen mode
        if (key & HidNpadButton_ZR) {
            app.window.image_fullscreen = !app.window.image_fullscreen;
            if (app.window.image_fullscreen) {
                s_toast_timer = TOAST_DURATION;  // Show toast when entering fullscreen
            }
        }
        
        if (!properties) {
            if (key & HidNpadButton_B) {
                ImageViewer::ClearTextures(app);
                ImageViewer::ClearPreloadedTextures(app);
                // Don't process deferred deletions here - this frame's draw commands
                // still reference the textures. They'll be freed at the start of
                // the next frame by CleanupDeferredDeletions() in MainWindow.
                app.window.zoom_factor = 1.0f;
                app.window.pan_offset_x = 0.0f;
                app.window.pan_offset_y = 0.0f;
                app.window.image_fullscreen = false;
                s_frames_since_navigation = 0;
                app.window.state = WINDOW_STATE_FILEBROWSER;
            }
            
            if (key & HidNpadButton_L) {
                ImageViewer::HandlePrev(app);
            }
            else if (key & HidNpadButton_R) {
                ImageViewer::HandleNext(app);
            }
        }
    }
}

namespace Windows {
    static void DrawImageViewerBottomBar(App &app) {
        const int lang = app.config.Lang();
        
        // Left-aligned items (minus button for exit)
        std::vector<BottomBar::HintItem> left_items = {
            {BottomBar::ButtonType::Minus, strings[lang][Lang::HintExit]}
        };
        
        // Right-aligned items
        std::vector<BottomBar::HintItem> right_items = {
            {BottomBar::ButtonType::CircleB, strings[lang][Lang::HintBack]},
            {BottomBar::ButtonType::CircleX, strings[lang][Lang::HintProperties]},
            {BottomBar::ButtonType::ShoulderL, strings[lang][Lang::HintPrev]},
            {BottomBar::ButtonType::ShoulderR, strings[lang][Lang::HintNext]},
            {BottomBar::ButtonType::DPadUp, strings[lang][Lang::HintZoomIn]},
            {BottomBar::ButtonType::DPadDown, strings[lang][Lang::HintZoomOut]},
            {BottomBar::ButtonType::ShoulderZR, strings[lang][Lang::HintFullscreen], app.window.image_fullscreen}
        };
        
        BottomBar::Config config;
        config.use_foreground_draw_list = true;
        config.draw_background = true;
        
        BottomBar::Draw(app.config, config, left_items, right_items);
    }
    
    void ImageViewer(App &app, bool &properties, bool &file_stat) {
        // Note: Deferred texture deletions are processed at the start of MainWindow
        // in window.cpp, which ensures they run every frame regardless of state.
        
        const float display_w = static_cast<float>(app.gui.display_width);
        const float display_h = static_cast<float>(app.gui.display_height);
        
        // Bottom bar height (must match DrawImageViewerBottomBar)
        const float bottom_bar_height = 45.0f;
        
        // In non-fullscreen mode, reduce window height to not overlap with bottom bar
        const float window_h = app.window.image_fullscreen ? display_h : (display_h - bottom_bar_height);
        
        // Set up window with appropriate height
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(display_w, window_h), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        
        // Remove window border in fullscreen mode
        if (app.window.image_fullscreen) {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        }
        
        // Always use NoTitleBar - filename is shown as toast overlay when enabled
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;
        if (app.window.image_fullscreen) {
            window_flags |= ImGuiWindowFlags_NoScrollbar;
        } else {
            window_flags |= ImGuiWindowFlags_HorizontalScrollbar;
        }
        
        if (ImGui::Begin(app.window.entries[app.window.selected].name, nullptr, window_flags)) {
            // Only render if we have valid textures
            if (!app.window.textures.empty() && app.window.textures[0].id != 0) {
                // Calculate zoom factor: in fullscreen mode, fit to screen
                float effective_zoom = app.window.zoom_factor;
                if (app.window.image_fullscreen) {
                    // Calculate scale to fit image within display
                    float scale_x = display_w / static_cast<float>(app.window.textures[0].width);
                    float scale_y = display_h / static_cast<float>(app.window.textures[0].height);
                    effective_zoom = (scale_x < scale_y) ? scale_x : scale_y;
                    // Don't upscale beyond 1:1 unless already zoomed
                    if (effective_zoom > 1.0f) effective_zoom = 1.0f;
                }
                
                float img_w = app.window.textures[0].width * effective_zoom;
                float img_h = app.window.textures[0].height * effective_zoom;
                
                // Calculate base cursor position (centered)
                ImVec2 cursor_pos = (ImGui::GetWindowSize() - ImVec2(img_w, img_h)) * 0.5f;
                
                // Apply R stick pan offset
                cursor_pos.x += app.window.pan_offset_x;
                cursor_pos.y += app.window.pan_offset_y;
                
                // Clamp pan offset so image doesn't go completely off screen
                float max_pan_x = img_w * 0.9f;
                float max_pan_y = img_h * 0.9f;
                if (app.window.pan_offset_x > max_pan_x) app.window.pan_offset_x = max_pan_x;
                if (app.window.pan_offset_x < -max_pan_x) app.window.pan_offset_x = -max_pan_x;
                if (app.window.pan_offset_y > max_pan_y) app.window.pan_offset_y = max_pan_y;
                if (app.window.pan_offset_y < -max_pan_y) app.window.pan_offset_y = -max_pan_y;
                
                ImGui::SetCursorPos(cursor_pos);
                    
                if (app.window.textures.size() > 1) {
                    svcSleepThread(app.window.textures[app.window.frame_count].delay);
                    float frame_w = app.window.textures[app.window.frame_count].width * effective_zoom;
                    float frame_h = app.window.textures[app.window.frame_count].height * effective_zoom;
                    ImGui::Image(static_cast<ImTextureID>(app.window.textures[app.window.frame_count].id), ImVec2(frame_w, frame_h));
                    app.window.frame_count++;
                    
                    if (app.window.frame_count == app.window.textures.size() - 1)
                        app.window.frame_count = 0;
                }
                else
                    ImGui::Image(static_cast<ImTextureID>(app.window.textures[0].id), ImVec2(img_w, img_h));
            }
        }
        
        if (!app.window.image_fullscreen) {
            Windows::DrawImageViewerBottomBar(app);
        }
        
        // Draw filename toast overlay (when enabled in settings)
        if (app.config.ImageFilename()) {
            Toast::DrawFilename(app.config, app.window.entries[app.window.selected].name);
        }
        
        // Draw fullscreen toast notification (temporary, shows when entering fullscreen)
        if (app.window.image_fullscreen && ImageViewer::s_toast_timer > 0.0f) {
            const int lang = app.config.Lang();
            
            // Fade out in the last 0.5 seconds
            float alpha = 1.0f;
            if (ImageViewer::s_toast_timer < 0.5f) {
                alpha = ImageViewer::s_toast_timer / 0.5f;
            }
            
            Toast::DrawCentered(app.gui, strings[lang][Lang::HintExitFullscreen], alpha);
            
            // Decrement timer
            ImageViewer::s_toast_timer -= ImGui::GetIO().DeltaTime;
        }
        
        // Pre-load adjacent images only after settling delay
        ImageViewer::PreloadAdjacentImages(app);

        if (properties && !app.window.textures.empty() && app.window.textures[0].id != 0)
            Popups::ImageProperties(app, properties, app.window.textures[0], file_stat);
        
        ImGui::End();
        ImGui::PopStyleVar(app.window.image_fullscreen ? 3 : 2);  // Pop WindowBorderSize (fullscreen only), WindowPadding, WindowRounding
    }
}
