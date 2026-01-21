#include <string>

#include "bottombar.hpp"
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

    // Helper to check if an entry is a valid image file
    static bool IsImageEntry(int index) {
        if (index < 0 || index >= static_cast<int>(data.entries.size()))
            return false;
        if (data.entries[index].type == FsDirEntryType_Dir)
            return false;
        const char* name = data.entries[index].name;
        return name[0] != '\0' && FS::GetFileType(name) == FileTypeImage;
    }

    // Find adjacent image file in given direction (direction: -1 = prev, +1 = next)
    static int FindAdjacentImageIndex(int from_index, int direction) {
        const int count = static_cast<int>(data.entries.size());
        
        // Search in direction from current position
        for (int i = from_index + direction; direction > 0 ? i < count : i >= 0; i += direction) {
            if (IsImageEntry(i))
                return i;
        }
        
        // Wrap around: search from opposite end
        int wrap_start = direction > 0 ? 0 : count - 1;
        int wrap_end = from_index;
        for (int i = wrap_start; direction > 0 ? i < wrap_end : i > wrap_end; i += direction) {
            if (IsImageEntry(i))
                return i;
        }
        
        return -1;  // No other images found
    }

    // Public API wrappers
    int FindPrevImageIndex(int from_index) { return FindAdjacentImageIndex(from_index, -1); }
    int FindNextImageIndex(int from_index) { return FindAdjacentImageIndex(from_index, +1); }

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

    // Navigate to adjacent image (direction: -1 = prev, +1 = next)
    static bool HandleNavigate(int direction) {
        int target_idx = FindAdjacentImageIndex(static_cast<int>(data.selected), direction);
        
        if (target_idx < 0)
            return false;  // No other images, stay on current
        
        // Reset pre-load delay counter
        s_frames_since_navigation = 0;
        
        // Select source/opposite preload buffers based on direction
        // When going prev (-1): source = textures_prev, opposite = textures_next
        // When going next (+1): source = textures_next, opposite = textures_prev
        auto& source_textures = direction < 0 ? data.textures_prev : data.textures_next;
        auto& opposite_textures = direction < 0 ? data.textures_next : data.textures_prev;
        int& source_preload_index = direction < 0 ? data.preload_prev_index : data.preload_next_index;
        int& opposite_preload_index = direction < 0 ? data.preload_next_index : data.preload_prev_index;
        
        // Check if we have this image pre-loaded and ready
        if (target_idx == source_preload_index && !source_textures.empty() && source_textures[0].id != 0) {
            // Hand over current image to become opposite preload (avoid re-decoding)
            // Free the old opposite preload first (it's now 2 images away, stale)
            FreeTextureVector(opposite_textures);
            
            // Move current -> opposite preload (the image we just left)
            opposite_textures = std::move(data.textures);
            opposite_preload_index = static_cast<int>(data.selected);
            
            // Move source preload -> current
            data.textures = std::move(source_textures);
            source_textures.clear();
            source_preload_index = -1;
            
            data.selected = target_idx;
            data.frame_count = 0;
            return true;
        }
        
        // Pre-load not available - load directly (keeps current image visible during load)
        std::vector<Tex> new_textures;
        char fs_path[FS_MAX_PATH + 1];
        if (std::snprintf(fs_path, FS_MAX_PATH, "%s/%s", cwd.c_str(), data.entries[target_idx].name) > 0) {
            if (Textures::LoadImageFile(fs_path, new_textures)) {
                // Hand over current image to become opposite preload (avoid re-decoding)
                FreeTextureVector(opposite_textures);
                opposite_textures = std::move(data.textures);
                opposite_preload_index = static_cast<int>(data.selected);
                
                // Set new current
                data.textures = std::move(new_textures);
                
                // Clear source preload (will be loaded by PreloadAdjacentImages)
                FreeTextureVector(source_textures);
                source_preload_index = -1;
                
                data.selected = target_idx;
                data.frame_count = 0;
                return true;
            }
        }
        
        return false;
    }

    // Public API wrappers
    bool HandlePrev(void) { return HandleNavigate(-1); }
    bool HandleNext(void) { return HandleNavigate(+1); }

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
            {BottomBar::ButtonType::ShoulderZR, strings[lang][Lang::HintFullscreen], data.image_fullscreen}
        };
        
        BottomBar::Config config;
        config.use_foreground_draw_list = true;
        config.draw_background = true;
        
        BottomBar::Draw(config, left_items, right_items);
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
            Toast::DrawFilename(data.entries[data.selected].name);
        }
        
        // Draw fullscreen toast notification (temporary, shows when entering fullscreen)
        if (data.image_fullscreen && ImageViewer::s_toast_timer > 0.0f) {
            const int lang = Config::GetLang();
            
            // Fade out in the last 0.5 seconds
            float alpha = 1.0f;
            if (ImageViewer::s_toast_timer < 0.5f) {
                alpha = ImageViewer::s_toast_timer / 0.5f;
            }
            
            Toast::DrawCentered(strings[lang][Lang::HintExitFullscreen], alpha);
            
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
