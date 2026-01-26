#include <algorithm>
#include <cstring>
#include <switch.h>

#include "config.hpp"
#include "fs.hpp"
#include "gui.hpp"
#include "services.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "popups.hpp"
#include "selection.hpp"
#include "tabs.hpp"
#include "windows.hpp"

namespace Windows {
    static bool image_properties = false, text_properties = false, file_stat = false;
    static int current_tab = -1;  // -1 = no forced selection, 0-2 = force select tab
    static int active_tab = 0;    // Track which tab is currently active
    
    int GetActiveTab(void) {
        return active_tab;
    }

    // Process any deferred texture deletions from the image viewer.
    // Called at the start of each frame to ensure textures are freed AFTER
    // the previous frame's draw commands have been executed by OpenGL.
    static void ProcessImageViewerCleanup(void) {
        ImageViewer::CleanupDeferredDeletions();
    }

    void SetupWindow(GUIService &gui_svc) {
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(static_cast<float>(gui_svc.display_width), static_cast<float>(gui_svc.display_height)), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    };
    
    void ExitWindow(void) {
        ImGui::End();
        ImGui::PopStyleVar();
    };
    

    // Draw a shoulder button indicator
    static constexpr float SHOULDER_BTN_WIDTH = 36.0f;
    static constexpr float SHOULDER_BTN_HEIGHT = 22.0f;
    static constexpr float SHOULDER_BTN_SPACING = 8.0f;  // Padding around button
    static constexpr float SHOULDER_BTN_CHAMFER = 8.0f;  // Chamfer size for corners
    
    static void DrawShoulderButton(ConfigService &config_svc, ImDrawList *draw_list, ImVec2 pos, const char *label, bool is_left) {
        // Theme-aware colors for shoulder buttons
        const bool dark_theme = GUI::IsCurrentThemeDark(config_svc);
        const ImU32 bg_color = dark_theme ? IM_COL32(60, 60, 60, 255) : IM_COL32(180, 180, 185, 255);
        const ImU32 text_color = dark_theme ? IM_COL32(255, 255, 255, 255) : IM_COL32(40, 40, 45, 255);
        
        float x1 = pos.x;
        float y1 = pos.y;
        float x2 = pos.x + SHOULDER_BTN_WIDTH;
        float y2 = pos.y + SHOULDER_BTN_HEIGHT;
        
        // Draw shape with one chamfered corner (top-left for L, top-right for R)
        ImVec2 points[6];
        if (is_left) {
            // L button: chamfer top-left corner
            points[0] = ImVec2(x1 + SHOULDER_BTN_CHAMFER, y1);  // Top edge start (after chamfer)
            points[1] = ImVec2(x2, y1);                         // Top-right
            points[2] = ImVec2(x2, y2);                         // Bottom-right
            points[3] = ImVec2(x1, y2);                         // Bottom-left
            points[4] = ImVec2(x1, y1 + SHOULDER_BTN_CHAMFER);  // Left edge (before chamfer)
            points[5] = ImVec2(x1 + SHOULDER_BTN_CHAMFER, y1);  // Close the chamfer
            draw_list->AddConvexPolyFilled(points, 5, bg_color);
        } else {
            // R button: chamfer top-right corner
            points[0] = ImVec2(x1, y1);                         // Top-left
            points[1] = ImVec2(x2 - SHOULDER_BTN_CHAMFER, y1);  // Top edge end (before chamfer)
            points[2] = ImVec2(x2, y1 + SHOULDER_BTN_CHAMFER);  // Right edge (after chamfer)
            points[3] = ImVec2(x2, y2);                         // Bottom-right
            points[4] = ImVec2(x1, y2);                         // Bottom-left
            draw_list->AddConvexPolyFilled(points, 5, bg_color);
        }
        
        ImVec2 text_size = ImGui::CalcTextSize(label);
        ImVec2 text_pos = ImVec2(
            pos.x + (SHOULDER_BTN_WIDTH - text_size.x) * 0.5f,
            pos.y + (SHOULDER_BTN_HEIGHT - text_size.y) * 0.5f
        );
        draw_list->AddText(text_pos, text_color, label);
    }

    // Status bar constants
    static constexpr float STATUS_BAR_PADDING = 10.0f;
    static constexpr float STATUS_ITEM_SPACING = 12.0f;
    static constexpr float ICON_HEIGHT = 16.0f;
    
    // Draw the WiFi signal strength indicator (4 bars + dB value)
    // rssi: signal strength in dBm, or 0 if unavailable (nifm fallback)
    static void DrawWiFiIndicator(ConfigService &config_svc, ImDrawList *draw_list, ImVec2 pos, int signal_bars, bool connected, s32 rssi) {
        const bool dark_theme = GUI::IsCurrentThemeDark(config_svc);
        const ImU32 bar_active_color = dark_theme ? IM_COL32(255, 255, 255, 255) : IM_COL32(40, 40, 45, 255);
        const ImU32 bar_inactive_color = dark_theme ? IM_COL32(80, 80, 80, 180) : IM_COL32(180, 180, 185, 180);
        const ImU32 text_color = dark_theme ? IM_COL32(255, 255, 255, 255) : IM_COL32(40, 40, 45, 255);
        const ImU32 disconnected_color = GUI::GetAccentColorU32(config_svc);  // Use accent color when disconnected
        
        const float bar_width = 3.0f;
        const float bar_spacing = 2.0f;
        const float base_height = 4.0f;
        const float height_step = 3.0f;
        const float bars_total_width = 4 * bar_width + 3 * bar_spacing;  // 18px
        
        // Draw 4 bars from left to right, increasing in height
        for (int i = 0; i < 4; i++) {
            float bar_height = base_height + (height_step * i);
            float x = pos.x + i * (bar_width + bar_spacing);
            float y = pos.y + ICON_HEIGHT - bar_height;
            
            ImU32 color;
            if (!connected) {
                color = disconnected_color;
            } else {
                color = (i < signal_bars) ? bar_active_color : bar_inactive_color;
            }
            
            draw_list->AddRectFilled(ImVec2(x, y), ImVec2(x + bar_width, pos.y + ICON_HEIGHT), color, 1.0f);
        }
        
        // Draw dB value text after the bars
        char db_text[16];
        if (!connected) {
            snprintf(db_text, sizeof(db_text), "--");
        } else if (rssi != 0) {
            snprintf(db_text, sizeof(db_text), "%ddB", rssi);
        } else {
            // nifm fallback - no RSSI available
            snprintf(db_text, sizeof(db_text), "OK");
        }
        ImVec2 text_size = ImGui::CalcTextSize(db_text);
        ImVec2 text_pos = ImVec2(pos.x + bars_total_width + 4.0f, pos.y + (ICON_HEIGHT - text_size.y) / 2.0f);
        draw_list->AddText(text_pos, connected ? text_color : disconnected_color, db_text);
    }
    
    // Draw the battery indicator with percentage
    static void DrawBatteryIndicator(ConfigService &config_svc, ImDrawList *draw_list, ImVec2 pos, u32 percent, bool is_charging) {
        const bool dark_theme = GUI::IsCurrentThemeDark(config_svc);
        const ImU32 outline_color = dark_theme ? IM_COL32(200, 200, 200, 255) : IM_COL32(60, 60, 65, 255);
        const ImU32 text_color = dark_theme ? IM_COL32(255, 255, 255, 255) : IM_COL32(40, 40, 45, 255);
        
        // Battery colors based on level
        ImU32 fill_color;
        if (is_charging) {
            fill_color = IM_COL32(100, 200, 100, 255);  // Green when charging
        } else if (percent <= 15) {
            fill_color = IM_COL32(220, 60, 60, 255);    // Red when low
        } else if (percent <= 30) {
            fill_color = IM_COL32(220, 180, 60, 255);   // Yellow/Orange when medium-low
        } else {
            fill_color = dark_theme ? IM_COL32(200, 200, 200, 255) : IM_COL32(80, 80, 85, 255);
        }
        
        // Battery body dimensions
        const float body_width = 22.0f;
        const float body_height = 12.0f;
        const float tip_width = 3.0f;
        const float tip_height = 6.0f;
        const float outline_thickness = 1.5f;
        const float fill_padding = 2.0f;
        
        float y_offset = (ICON_HEIGHT - body_height) / 2.0f;
        ImVec2 body_min = ImVec2(pos.x, pos.y + y_offset);
        ImVec2 body_max = ImVec2(pos.x + body_width, pos.y + y_offset + body_height);
        
        // Draw battery outline
        draw_list->AddRect(body_min, body_max, outline_color, 2.0f, 0, outline_thickness);
        
        // Draw battery tip (positive terminal)
        float tip_y = pos.y + y_offset + (body_height - tip_height) / 2.0f;
        draw_list->AddRectFilled(
            ImVec2(body_max.x, tip_y),
            ImVec2(body_max.x + tip_width, tip_y + tip_height),
            outline_color, 1.0f
        );
        
        // Draw battery fill
        float fill_width = (body_width - 2 * fill_padding - outline_thickness) * (percent / 100.0f);
        if (fill_width > 0) {
            draw_list->AddRectFilled(
                ImVec2(body_min.x + fill_padding + outline_thickness / 2, body_min.y + fill_padding + outline_thickness / 2),
                ImVec2(body_min.x + fill_padding + outline_thickness / 2 + fill_width, body_max.y - fill_padding - outline_thickness / 2),
                fill_color, 1.0f
            );
        }
        
        // Draw charging bolt if charging
        if (is_charging) {
            const ImU32 bolt_color = IM_COL32(255, 220, 50, 255);
            float cx = pos.x + body_width / 2.0f;
            float cy = pos.y + y_offset + body_height / 2.0f;
            // Simple lightning bolt shape
            ImVec2 bolt_points[7] = {
                ImVec2(cx + 1, cy - 4),
                ImVec2(cx - 2, cy),
                ImVec2(cx, cy),
                ImVec2(cx - 1, cy + 4),
                ImVec2(cx + 2, cy),
                ImVec2(cx, cy),
                ImVec2(cx + 1, cy - 4)
            };
            draw_list->AddPolyline(bolt_points, 7, bolt_color, ImDrawFlags_None, 1.5f);
        }
        
        // Draw percentage text
        char percent_text[8];
        snprintf(percent_text, sizeof(percent_text), "%u%%", percent);
        ImVec2 text_size = ImGui::CalcTextSize(percent_text);
        ImVec2 text_pos = ImVec2(pos.x + body_width + tip_width + 4.0f, pos.y + (ICON_HEIGHT - text_size.y) / 2.0f);
        draw_list->AddText(text_pos, text_color, percent_text);
    }
    
    // Draw the dock/resolution indicator with corner brackets around the text
    // is_forced: true when resolution is manually set and differs from auto-detected
    static void DrawDockIndicator(ConfigService &config_svc, ImDrawList *draw_list, ImVec2 pos, bool is_1080p, bool is_forced) {
        const bool dark_theme = GUI::IsCurrentThemeDark(config_svc);
        const ImU32 normal_color = dark_theme ? IM_COL32(200, 200, 200, 255) : IM_COL32(60, 60, 65, 255);
        const ImU32 accent_color = GUI::GetAccentColorU32(config_svc);
        
        // Use accent color when resolution is forced (overriding auto)
        const ImU32 indicator_color = is_forced ? accent_color : normal_color;
        
        // Get resolution text and calculate dimensions
        const char* res_text = is_1080p ? "1080p" : "720p";
        ImVec2 text_size = ImGui::CalcTextSize(res_text);
        
        // Padding around text for the corner brackets
        const float padding_x = 4.0f;
        const float padding_y = 2.0f;
        const float corner_size = 4.0f;
        const float line_thickness = 1.5f;
        
        // Calculate box dimensions based on text size
        float box_width = text_size.x + padding_x * 2;
        float box_height = text_size.y + padding_y * 2;
        
        // Center vertically
        float y_offset = (ICON_HEIGHT - box_height) / 2.0f;
        
        // Box coordinates
        float box_left = pos.x;
        float box_right = pos.x + box_width;
        float box_top = pos.y + y_offset;
        float box_bottom = pos.y + y_offset + box_height;
        
        // Draw corner brackets around the text
        // Top-left corner
        draw_list->AddLine(
            ImVec2(box_left, box_top + corner_size),
            ImVec2(box_left, box_top),
            indicator_color, line_thickness
        );
        draw_list->AddLine(
            ImVec2(box_left, box_top),
            ImVec2(box_left + corner_size, box_top),
            indicator_color, line_thickness
        );
        
        // Top-right corner
        draw_list->AddLine(
            ImVec2(box_right - corner_size, box_top),
            ImVec2(box_right, box_top),
            indicator_color, line_thickness
        );
        draw_list->AddLine(
            ImVec2(box_right, box_top),
            ImVec2(box_right, box_top + corner_size),
            indicator_color, line_thickness
        );
        
        // Bottom-left corner
        draw_list->AddLine(
            ImVec2(box_left, box_bottom - corner_size),
            ImVec2(box_left, box_bottom),
            indicator_color, line_thickness
        );
        draw_list->AddLine(
            ImVec2(box_left, box_bottom),
            ImVec2(box_left + corner_size, box_bottom),
            indicator_color, line_thickness
        );
        
        // Bottom-right corner
        draw_list->AddLine(
            ImVec2(box_right - corner_size, box_bottom),
            ImVec2(box_right, box_bottom),
            indicator_color, line_thickness
        );
        draw_list->AddLine(
            ImVec2(box_right, box_bottom - corner_size),
            ImVec2(box_right, box_bottom),
            indicator_color, line_thickness
        );
        
        // Draw resolution text centered within the brackets
        ImVec2 text_pos = ImVec2(pos.x + padding_x, pos.y + (ICON_HEIGHT - text_size.y) / 2.0f);
        draw_list->AddText(text_pos, indicator_color, res_text);
    }
    
    // Returns the total width of the status bar
    static float GetStatusBarWidth(void) {
        // WiFi: 18px (bars) + 4 (spacing) + ~40px for dB text (e.g. "-50dB")
        // Dock: corner brackets around text (~50px for "1080p" + padding)
        // Battery: 22 (body) + 3 (tip) + 4 (spacing) + text (~30px for "100%")
        // Order: WiFi, Res, Bat (with 20% extra space for res and battery)
        return 62.0f + STATUS_ITEM_SPACING + 78.0f + STATUS_ITEM_SPACING + 64.0f;
    }
    
    // Draw the Applet Mode banner centered in the title bar
    static void DrawAppletModeBanner(ImDrawList *draw_list, ImVec2 title_bar_min, ImVec2 title_bar_max) {
        const char* banner_text = "Applet Mode";
        
        ImVec2 text_size = ImGui::CalcTextSize(banner_text);
        
        // Calculate centered position
        float center_x = (title_bar_min.x + title_bar_max.x) / 2.0f;
        float center_y = (title_bar_min.y + title_bar_max.y) / 2.0f;
        ImVec2 text_pos = ImVec2(center_x - text_size.x / 2.0f, center_y - text_size.y / 2.0f);
        
        // Red color for warning
        ImU32 banner_color = IM_COL32(255, 60, 60, 255);
        
        draw_list->AddText(text_pos, banner_color, banner_text);
    }
    
    // Draw the complete status bar in the title bar area
    static void DrawStatusBar(App &app, ImDrawList *draw_list, ImVec2 title_bar_max) {
        // Get battery info
        u32 battery_percent = 100;
        bool is_charging = false;
        
        if (R_SUCCEEDED(psmGetBatteryChargePercentage(&battery_percent))) {
            // Clamp to valid range
            if (battery_percent > 100) battery_percent = 100;
        }
        
        // Check if charging (when docked or connected to charger)
        PsmChargerType charger_type = PsmChargerType_Unconnected;
        if (R_SUCCEEDED(psmGetChargerType(&charger_type))) {
            is_charging = (charger_type != PsmChargerType_Unconnected);
        }
        
        // Get WiFi signal strength
        int wifi_bars = 0;
        bool wifi_connected = false;
        s32 rssi = 0;
        
        // Try wlaninfo first (provides RSSI for signal strength bars)
        if (R_SUCCEEDED(wlaninfGetRSSI(&rssi))) {
            wifi_connected = true;
            // Convert RSSI to bars (typical range: -90 dBm to -30 dBm)
            // -30 to -50: 4 bars (excellent)
            // -50 to -60: 3 bars (good)
            // -60 to -70: 2 bars (fair)
            // -70 to -90: 1 bar (weak)
            // below -90: 0 bars (very weak)
            if (rssi >= -50) wifi_bars = 4;
            else if (rssi >= -60) wifi_bars = 3;
            else if (rssi >= -70) wifi_bars = 2;
            else if (rssi >= -80) wifi_bars = 1;
            else wifi_bars = 0;
        } else {
            // Fallback for Mariko devices where wlaninfo isn't available
            // Use nifm to check internet connection status (no signal strength info)
            NifmInternetConnectionStatus status;
            if (R_SUCCEEDED(nifmGetInternetConnectionStatus(nullptr, nullptr, &status))) {
                wifi_connected = (status == NifmInternetConnectionStatus_Connected);
                // Show full bars when connected since we can't determine actual signal strength
                wifi_bars = wifi_connected ? 4 : 0;
            }
        }
        
        // Check actual display resolution (not just dock state, as user may have forced a resolution)
        bool is_1080p = (app.gui.display_height >= 1080);
        
        // Check if resolution is forced (overriding what auto would detect)
        // Auto mode follows dock state: docked = 1080p, handheld = 720p
        bool is_docked = GUI::IsDocked();
        bool auto_would_be_1080p = is_docked;
        bool is_resolution_forced = false;
        if (app.config.ResolutionMode() == ResolutionMode_1080p && !auto_would_be_1080p) {
            // User forced 1080p while handheld (auto would be 720p)
            is_resolution_forced = true;
        } else if (app.config.ResolutionMode() == ResolutionMode_720p && auto_would_be_1080p) {
            // User forced 720p while docked (auto would be 1080p)
            is_resolution_forced = true;
        }
        
        // Calculate positions (right-aligned)
        float status_width = GetStatusBarWidth();
        float start_x = title_bar_max.x - status_width - STATUS_BAR_PADDING;
        float y = title_bar_max.y - ICON_HEIGHT - 6.0f;  // Vertically center in title bar
        
        // Draw WiFi indicator with dB value
        DrawWiFiIndicator(app.config, draw_list, ImVec2(start_x, y), wifi_bars, wifi_connected, rssi);
        start_x += 62.0f + STATUS_ITEM_SPACING;
        
        // Draw dock/resolution indicator (highlighted if resolution is forced)
        DrawDockIndicator(app.config, draw_list, ImVec2(start_x, y), is_1080p, is_resolution_forced);
        start_x += 78.0f + STATUS_ITEM_SPACING;
        
        // Draw battery indicator
        DrawBatteryIndicator(app.config, draw_list, ImVec2(start_x, y), battery_percent, is_charging);
    }

    void MainWindow(App &app, u64 &key, bool progress) {
        // Process deferred image texture deletions at the START of each frame,
        // AFTER the previous frame's GPU commands have executed (via eglSwapBuffers).
        // This prevents the "font texture flash" when exiting the image viewer.
        ProcessImageViewerCleanup();
        
        SelectionStore selection(app.selection);
        
        Windows::SetupWindow(app.gui);
        if (ImGui::Begin("NX-Shell", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
            ImDrawList *draw_list = ImGui::GetWindowDrawList();
            
            // Draw status bar (battery, WiFi, dock) in the title bar area
            // Use GetForegroundDrawList() because the title bar is outside the window's
            // InnerClipRect that gets pushed after Begin() - window draw list would be clipped
            // Skip drawing when in fullscreen image viewer mode (user wants distraction-free viewing)
            ImGuiWindow* window = ImGui::GetCurrentWindow();
            if (window && !(app.window.state == WINDOW_STATE_IMAGEVIEWER && app.window.image_fullscreen)) {
                ImVec2 title_bar_min = ImVec2(window->Pos.x, window->Pos.y);
                ImVec2 title_bar_max = ImVec2(window->Pos.x + window->Size.x, window->Pos.y + ImGui::GetFrameHeight());
                DrawStatusBar(app, ImGui::GetForegroundDrawList(), title_bar_max);
                
                // Draw applet mode warning banner if running in applet mode
                if (GUI::IsAppletMode()) {
                    DrawAppletModeBanner(ImGui::GetForegroundDrawList(), title_bar_min, title_bar_max);
                }
            }
            
            ImVec2 cursor_start = ImGui::GetCursorScreenPos();
            
            // Draw L button on the left
            ImVec2 l_btn_pos = ImVec2(cursor_start.x, cursor_start.y + 2.0f);
            DrawShoulderButton(app.config, draw_list, l_btn_pos, "L", true);
            
            // Move cursor past L button
            ImGui::SetCursorScreenPos(ImVec2(cursor_start.x + SHOULDER_BTN_WIDTH + SHOULDER_BTN_SPACING, cursor_start.y));
            
            if (ImGui::BeginTabBar("NX-Shell-tabs", ImGuiTabBarFlags_None)) {
                // Track active tab based on which tab content is rendered
                Tabs::FileBrowser(app, current_tab, active_tab);
                Tabs::Settings(app, current_tab, active_tab);
                Tabs::About(app, current_tab, active_tab);
                
                // Get tab bar info before ending
                ImGuiTabBar* tab_bar = ImGui::GetCurrentTabBar();
                float tabs_width = tab_bar ? tab_bar->WidthAllTabs : 0.0f;
                float tab_bar_height = tab_bar ? tab_bar->BarRect.GetHeight() : SHOULDER_BTN_HEIGHT;
                
                ImGui::EndTabBar();
                
                // Draw R button right after the last tab
                ImVec2 r_btn_pos = ImVec2(cursor_start.x + SHOULDER_BTN_WIDTH + SHOULDER_BTN_SPACING + tabs_width + SHOULDER_BTN_SPACING, cursor_start.y + 2.0f);
                DrawShoulderButton(app.config, draw_list, r_btn_pos, "R", false);
                
                // Draw underline extending under both L and R buttons
                ImU32 line_color = ImGui::GetColorU32(ImGuiCol_TabSelected);
                float line_y = cursor_start.y + tab_bar_height - 1.0f;
                draw_list->AddLine(
                    ImVec2(cursor_start.x, line_y),
                    ImVec2(cursor_start.x + SHOULDER_BTN_WIDTH + SHOULDER_BTN_SPACING, line_y),
                    line_color, 1.0f
                );
                draw_list->AddLine(
                    ImVec2(r_btn_pos.x - SHOULDER_BTN_SPACING, line_y),
                    ImVec2(r_btn_pos.x + SHOULDER_BTN_WIDTH, line_y),
                    line_color, 1.0f
                );
            }
        }
        Windows::ExitWindow();
        
        // Handle L/R button tab switching - only when in file browser (not image viewer, text reader, etc.)
        if (app.window.state == WINDOW_STATE_FILEBROWSER) {
            if (key & HidNpadButton_L) {
                int new_tab = (active_tab - 1 + TAB_COUNT) % TAB_COUNT;
                current_tab = new_tab;
                // Request focus on the appropriate element for each tab
                switch (new_tab) {
                    case 0: Tabs::RequestFileBrowserFocus(app.fs); break;
                    case 1: Tabs::RequestSettingsFocus(); break;
                    case 2: Tabs::RequestAboutFocus(); break;
                }
            }
            if (key & HidNpadButton_R) {
                int new_tab = (active_tab + 1) % TAB_COUNT;
                current_tab = new_tab;
                // Request focus on the appropriate element for each tab
                switch (new_tab) {
                    case 0: Tabs::RequestFileBrowserFocus(app.fs); break;
                    case 1: Tabs::RequestSettingsFocus(); break;
                    case 2: Tabs::RequestAboutFocus(); break;
                }
            }
        }

        if (progress)
            return;

        switch (app.window.state) {
            case WINDOW_STATE_OPTIONS:
                Popups::OptionsPopup(app);
                break;

            case WINDOW_STATE_PROPERTIES:
                Popups::FilePropertiesPopup(app, file_stat);
                break;
            
            case WINDOW_STATE_DELETE:
                Popups::DeletePopup(app);
                break;

            case WINDOW_STATE_ARCHIVEEXTRACT:
                Popups::ArchivePopup(app);
                break;

            case WINDOW_STATE_REPLACE:
                Popups::ReplacePopup(app, Popups::IsPendingReplaceMove());
                break;

            case WINDOW_STATE_MULTI_REPLACE:
                Popups::MultiReplacePopup(app, Popups::IsPendingReplaceMove(), 
                                          Popups::GetMultiConflictCount(), 
                                          Popups::GetMultiTotalCount());
                break;

            case WINDOW_STATE_IMAGEVIEWER:
                Windows::ImageViewer(app, image_properties, file_stat);
                ImageViewer::HandleControls(app, key, image_properties);
                break;

            case WINDOW_STATE_TEXTREADER:
                Windows::TextReader(app, text_properties, file_stat);
                TextReader::HandleControls(app, key, text_properties);
                break;

            case WINDOW_STATE_OPENMODE:
                {
                    static bool show_openmode_popup = true;
                    int mode_result = Popups::OpenModePopup(app, show_openmode_popup);
                    
                    if (mode_result == 1) {
                        // Open as text
                        std::string path = FS::BuildPath(app.fs, app.window.entries[app.window.selected]);
                        if (TextReader::LoadFile(app, path)) {
                            TextReader::SetHexMode(false);
                            app.window.state = WINDOW_STATE_TEXTREADER;
                        } else {
                            app.window.state = WINDOW_STATE_FILEBROWSER;
                        }
                        show_openmode_popup = true;  // Reset for next time
                    } else if (mode_result == 2) {
                        // Open as hex
                        std::string path = FS::BuildPath(app.fs, app.window.entries[app.window.selected]);
                        if (TextReader::LoadFile(app, path)) {
                            TextReader::SetHexMode(true);
                            app.window.state = WINDOW_STATE_TEXTREADER;
                        } else {
                            app.window.state = WINDOW_STATE_FILEBROWSER;
                        }
                        show_openmode_popup = true;  // Reset for next time
                    } else if (!show_openmode_popup) {
                        // Cancelled
                        app.window.state = WINDOW_STATE_FILEBROWSER;
                        show_openmode_popup = true;  // Reset for next time
                    }
                }
                break;

            default:
                break;
        }

        if ((key & HidNpadButton_X) && (app.window.state == WINDOW_STATE_FILEBROWSER) && (active_tab == 0) && !FS::IsAtPartitionRoot(app.fs))
            app.window.state = WINDOW_STATE_OPTIONS;
        
        // Plus button opens the device selector when in file browser
        if ((key & HidNpadButton_Plus) && (app.window.state == WINDOW_STATE_FILEBROWSER) && (active_tab == 0))
            Tabs::RequestDeviceCombo();
        
        // ZR button toggles details view (size, date modified columns)
        if ((key & HidNpadButton_ZR) && (app.window.state == WINDOW_STATE_FILEBROWSER) && (active_tab == 0))
            Tabs::ToggleDetails(app);

        if ((key & HidNpadButton_Y) && (app.window.state == WINDOW_STATE_FILEBROWSER) && (active_tab == 0)) {
            // Toggle selection using SelectionStore
            if ((std::strncmp(app.window.entries[app.window.selected].name, "..", 2)) != 0) {
                std::string selected_path = app.fs.device + app.fs.cwd;
                if (!selected_path.empty() && selected_path.back() != '/')
                    selected_path += "/";
                selected_path += app.window.entries[app.window.selected].name;
                
                // For folders, use recursive selection/deselection
                // This allows users to later unselect individual items within
                if (app.window.entries[app.window.selected].type == FsDirEntryType_Dir) {
                    selection.ToggleFolder(selected_path);
                } else {
                    selection.Toggle(selected_path);
                }
            }
        }

        if (key & HidNpadButton_B) {
            // Don't process B button if ImGui had a popup/combo open when B was pressed
            // (ImGui already used B to close that popup, so we shouldn't also switch tabs)
            bool popup_was_open = GUI::WasPopupOpenOnBPress();
            
            switch(app.window.state) {
                case WINDOW_STATE_FILEBROWSER:
                    // B button navigates to parent directory when in file browser tab
                    // or jumps back to file browser from Settings/About tabs
                    if (active_tab == 0)
                        Tabs::RequestParentDirectory();
                    else if ((active_tab == 1 || active_tab == 2) && !popup_was_open) {
                        // Jump back to file browser tab (but not if a combo/popup was open)
                        current_tab = 0;
                        Tabs::RequestFileBrowserFocus(app.fs);
                    }
                    break;
                
                case WINDOW_STATE_OPTIONS:
                    app.window.state = WINDOW_STATE_FILEBROWSER;
                    break;

                case WINDOW_STATE_PROPERTIES:
                    app.window.state = WINDOW_STATE_OPTIONS;
                    file_stat = false;
                    break;
                
                case WINDOW_STATE_DELETE:
                    app.window.state = WINDOW_STATE_OPTIONS;
                    break;

                case WINDOW_STATE_REPLACE:
                    app.window.state = WINDOW_STATE_OPTIONS;
                    break;

                case WINDOW_STATE_MULTI_REPLACE:
                    Popups::ClearPendingMultiOperation(app.fs);
                    app.window.state = WINDOW_STATE_OPTIONS;
                    break;

                case WINDOW_STATE_ARCHIVEEXTRACT:
                    app.window.state = WINDOW_STATE_FILEBROWSER;
                    break;

                case WINDOW_STATE_OPENMODE:
                    app.window.state = WINDOW_STATE_FILEBROWSER;
                    break;

                case WINDOW_STATE_IMAGEVIEWER:
                    if (image_properties) {
                        image_properties = false;
                        file_stat = false;
                    }
                    else {
                        ImageViewer::ClearTextures(app);
                        app.window.state = WINDOW_STATE_FILEBROWSER;
                    }
                    
                    break;

                case WINDOW_STATE_TEXTREADER:
                    if (text_properties) {
                        text_properties = false;
                        file_stat = false;
                    }
                    else {
                        TextReader::Clear(app);
                        app.window.state = WINDOW_STATE_FILEBROWSER;
                    }
                    
                    break;

                default:
                    break;
            }
        }
    }
}
