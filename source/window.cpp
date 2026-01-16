#include <algorithm>
#include <cstring>

#include "config.hpp"
#include "gui.hpp"
#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"
#include "popups.hpp"
#include "tabs.hpp"
#include "windows.hpp"

WindowData data;

namespace Windows {
    static bool image_properties = false, file_stat = false;
    static int current_tab = -1;  // -1 = no forced selection, 0-2 = force select tab
    static int active_tab = 0;    // Track which tab is currently active

    void SetupWindow(void) {
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(static_cast<float>(GUI::display_width), static_cast<float>(GUI::display_height)), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    };
    
    void ExitWindow(void) {
        ImGui::End();
        ImGui::PopStyleVar();
    };
    
    void ResetCheckbox(WindowData &data) {
        data.checkbox_data.checked.clear();
        data.checkbox_data.checked_copy.clear();
        data.checkbox_data.checked.resize(data.entries.size());
        data.checkbox_data.checked.assign(data.checkbox_data.checked.size(), false);
        data.checkbox_data.cwd = "";
        data.checkbox_data.count = 0;
    };

    // Draw a shoulder button indicator
    static constexpr float SHOULDER_BTN_WIDTH = 28.0f;
    static constexpr float SHOULDER_BTN_HEIGHT = 22.0f;
    static constexpr float SHOULDER_BTN_SPACING = 8.0f;  // Padding around button
    
    static void DrawShoulderButton(ImDrawList *draw_list, ImVec2 pos, const char *label) {
        const float rounding = 4.0f;
        const ImU32 bg_color = IM_COL32(60, 60, 60, 255);
        const ImU32 text_color = IM_COL32(255, 255, 255, 255);
        
        ImVec2 p1 = pos;
        ImVec2 p2 = ImVec2(pos.x + SHOULDER_BTN_WIDTH, pos.y + SHOULDER_BTN_HEIGHT);
        
        draw_list->AddRectFilled(p1, p2, bg_color, rounding);
        
        ImVec2 text_size = ImGui::CalcTextSize(label);
        ImVec2 text_pos = ImVec2(
            pos.x + (SHOULDER_BTN_WIDTH - text_size.x) * 0.5f,
            pos.y + (SHOULDER_BTN_HEIGHT - text_size.y) * 0.5f
        );
        draw_list->AddText(text_pos, text_color, label);
    }

    void MainWindow(WindowData &data, u64 &key, bool progress) {
        Windows::SetupWindow();
        if (ImGui::Begin("NX-Shell", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
            ImDrawList *draw_list = ImGui::GetWindowDrawList();
            ImVec2 cursor_start = ImGui::GetCursorScreenPos();
            
            // Draw L button on the left
            ImVec2 l_btn_pos = ImVec2(cursor_start.x, cursor_start.y + 2.0f);
            DrawShoulderButton(draw_list, l_btn_pos, "L");
            
            // Move cursor past L button
            ImGui::SetCursorScreenPos(ImVec2(cursor_start.x + SHOULDER_BTN_WIDTH + SHOULDER_BTN_SPACING, cursor_start.y));
            
            if (ImGui::BeginTabBar("NX-Shell-tabs", ImGuiTabBarFlags_None)) {
                // Track active tab based on which tab content is rendered
                Tabs::FileBrowser(data, current_tab, active_tab);
                Tabs::Settings(data, current_tab, active_tab);
                Tabs::About(data, current_tab, active_tab);
                
                // Get tab bar info before ending
                ImGuiTabBar* tab_bar = ImGui::GetCurrentTabBar();
                float tabs_width = tab_bar ? tab_bar->WidthAllTabs : 0.0f;
                float tab_bar_height = tab_bar ? tab_bar->BarRect.GetHeight() : SHOULDER_BTN_HEIGHT;
                
                ImGui::EndTabBar();
                
                // Draw R button right after the last tab
                ImVec2 r_btn_pos = ImVec2(cursor_start.x + SHOULDER_BTN_WIDTH + SHOULDER_BTN_SPACING + tabs_width + SHOULDER_BTN_SPACING, cursor_start.y + 2.0f);
                DrawShoulderButton(draw_list, r_btn_pos, "R");
                
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
        
        // Handle L/R button tab switching
        if (key & HidNpadButton_L) {
            int new_tab = (active_tab - 1 + TAB_COUNT) % TAB_COUNT;
            current_tab = new_tab;
            // Request focus on the appropriate element for each tab
            switch (new_tab) {
                case 0: Tabs::RequestFileBrowserFocus(); break;
                case 1: Tabs::RequestSettingsFocus(); break;
                case 2: Tabs::RequestAboutFocus(); break;
            }
        }
        if (key & HidNpadButton_R) {
            int new_tab = (active_tab + 1) % TAB_COUNT;
            current_tab = new_tab;
            // Request focus on the appropriate element for each tab
            switch (new_tab) {
                case 0: Tabs::RequestFileBrowserFocus(); break;
                case 1: Tabs::RequestSettingsFocus(); break;
                case 2: Tabs::RequestAboutFocus(); break;
            }
        }

        if (progress)
            return;

        switch (data.state) {
            case WINDOW_STATE_OPTIONS:
                Popups::OptionsPopup(data);
                break;

            case WINDOW_STATE_PROPERTIES:
                Popups::FilePropertiesPopup(data, file_stat);
                break;
            
            case WINDOW_STATE_DELETE:
                Popups::DeletePopup(data);
                break;

            case WINDOW_STATE_IMAGEVIEWER:
                Windows::ImageViewer(image_properties, file_stat);
                ImageViewer::HandleControls(key, image_properties);
                break;

            default:
                break;
        }

        if ((key & HidNpadButton_X) && (data.state == WINDOW_STATE_FILEBROWSER))
            data.state = WINDOW_STATE_OPTIONS;
        
        // Plus button opens the device selector when in file browser
        if ((key & HidNpadButton_Plus) && (data.state == WINDOW_STATE_FILEBROWSER) && (active_tab == 0))
            Tabs::RequestDeviceCombo();

        if (key & HidNpadButton_Y) {
            if ((data.checkbox_data.cwd.length() != 0) && (data.checkbox_data.cwd != cwd))
                Windows::ResetCheckbox(data);
            
            if ((std::strncmp(data.entries[data.selected].name, "..", 2)) != 0) {
                data.checkbox_data.cwd = cwd;
                data.checkbox_data.device = device;
                data.checkbox_data.checked.at(data.selected) = !data.checkbox_data.checked.at(data.selected);
                data.checkbox_data.count = std::count(data.checkbox_data.checked.begin(), data.checkbox_data.checked.end(), true);
            }
        }

        if (key & HidNpadButton_B) {
            switch(data.state) {
                case WINDOW_STATE_FILEBROWSER:
                    // B button navigates to parent directory when in file browser
                    if (active_tab == 0)
                        Tabs::RequestParentDirectory();
                    break;
                
                case WINDOW_STATE_OPTIONS:
                    data.state = WINDOW_STATE_FILEBROWSER;
                    break;

                case WINDOW_STATE_PROPERTIES:
                    data.state = WINDOW_STATE_OPTIONS;
                    file_stat = false;
                    break;
                
                case WINDOW_STATE_DELETE:
                    data.state = WINDOW_STATE_OPTIONS;
                    break;

                case WINDOW_STATE_IMAGEVIEWER:
                    if (image_properties) {
                        image_properties = false;
                        file_stat = false;
                    }
                    else {
                        ImageViewer::ClearTextures();
                        data.state = WINDOW_STATE_FILEBROWSER;
                    }
                    
                    break;

                default:
                    break;
            }
        }
    }
}
