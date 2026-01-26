#include "config.hpp"
#include "fs.hpp"
#include "gui.hpp"
#include "services.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_switch.hpp"
#include "keyboard.hpp"
#include "language.hpp"
#include "log.hpp"
#include "net.hpp"
#include "popups.hpp"
#include "tabs.hpp"
#include "tabs_internal.hpp"
#include "textures.hpp"
#include "usb.hpp"

static bool need_focus_settings = false;

// Helper to disable UI elements in applet mode
static void BeginAppletDisabled(void) {
    if (GUI::IsAppletMode()) {
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
    }
}

static void EndAppletDisabled(void) {
    if (GUI::IsAppletMode()) {
        ImGui::PopItemFlag();
        ImGui::PopStyleVar();
    }
}

namespace Tabs {
    void RequestSettingsFocus(void) {
        need_focus_settings = true;
    }

    static bool unmount_popup = false;
    static bool reset_settings_popup = false;
    
    void Settings(App &app, int &current_tab, int &active_tab) {
        ImGuiTabItemFlags flags = (current_tab == 1) ? ImGuiTabItemFlags_SetSelected : 0;
        if (current_tab == 1) current_tab = -1; // Reset after applying
        
        if (ImGui::BeginTabItem(strings[app.config.Lang()][Lang::TabSettings], nullptr, flags)) {
            active_tab = 1;  // Update active tab when this tab is visible
            const int lang = app.config.Lang();
            
            // Reserve space for button bar at the bottom
            const float button_bar_height = 40.0f;
            float available_height = ImGui::GetContentRegionAvail().y - button_bar_height;
            
            // Begin scrollable child region for settings content
            ImGui::BeginChild("SettingsContent", ImVec2(0, available_height), false);
            
            // Language selector
            Internal::Indent(strings[lang][Lang::SettingsLanguageTitle]);
            
            BeginAppletDisabled();

            // Show all available languages, alphabetically sorted
            struct LanguageOption {
                const char* name;
                int value;  // -1 = Auto, or language index
            };
            
            static const LanguageOption supported_languages[] = {
                {" Auto", LANG_AUTO},
                {" Chinese (Simplified)", 6},
                {" Chinese (Traditional)", 11},
                {" Dutch", 8},
                {" English", 1},
                {" French", 2},
                {" German", 3},
                {" Italian", 4},
                {" Japanese", 0},
                {" Korean", 7},
                {" Portuguese", 9},
                {" Russian", 10},
                {" Spanish", 5}
            };
            
            static const int num_languages = sizeof(supported_languages) / sizeof(supported_languages[0]);

            // Find current selection index
            int current_selection = 0;
            for (int i = 0; i < num_languages; i++) {
                if (supported_languages[i].value == app.config.normal.lang) {
                    current_selection = i;
                    break;
                }
            }
            
            // Focus language combo when tab is switched via L/R
            if (need_focus_settings) {
                ImGui::SetKeyboardFocusHere();
                need_focus_settings = false;
            }
            
            ImGui::PushItemWidth(250.f);
            bool language_selected = false;
            if (ImGui::BeginCombo("##language_combo", supported_languages[current_selection].name)) {
                for (int i = 0; i < num_languages; i++) {
                    const bool is_selected = (current_selection == i);
                    if (ImGui::Selectable(supported_languages[i].name, is_selected)) {
                        Log::Debug("Language change: old_lang=%d, new_lang=%d, show_stats_before=%d\n", 
                                   app.config.normal.lang, supported_languages[i].value, app.config.normal.show_stats);
                        app.config.normal.lang = supported_languages[i].value;
                        Config::Save(app.config, app.fs);
                        Log::Debug("After Config::Save: show_stats=%d\n", app.config.normal.show_stats);
                        GUI::ResetUIState(app);  // Force full UI refresh for new language
                        Log::Debug("After ResetUIState: show_stats=%d\n", app.config.normal.show_stats);
                        language_selected = true;
                        current_tab = 1;  // Stay on Settings tab after language change
                    }
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            // After language selection, request focus back to settings to prevent tab navigation
            if (language_selected) {
                need_focus_settings = true;
            }
            ImGui::PopItemWidth();
            
            EndAppletDisabled();

            Internal::Separator();

            // USB unmount
            BeginAppletDisabled();
            Internal::Indent(strings[lang][Lang::SettingsUSBTitle]);

            if (!USB::Connected()) {
                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
            }

            const char* usb_text = strings[lang][Lang::SettingsUSBUnmount];
            float usb_button_width = std::max(200.0f, ImGui::CalcTextSize(usb_text).x + 40.0f);
            if (ImGui::Button(usb_text, ImVec2(usb_button_width, 50)))
                unmount_popup = true;
            
            if (!USB::Connected()) {
                ImGui::PopItemFlag();
                ImGui::PopStyleVar();
            }
            
            EndAppletDisabled();

            Internal::Separator();

            // Image viewer options
            BeginAppletDisabled();
            Internal::Indent(strings[lang][Lang::SettingsImageViewTitle]);

            ImGui::PushID("image_filename");
            if (ImGui::Checkbox(strings[lang][Lang::SettingsImageViewFilenameToggle], std::addressof(app.config.normal.image_filename)))
                Config::Save(app.config, app.fs);
            ImGui::PopID();

            ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
            
            ImGui::PushID("enter_images_fullscreen");
            if (ImGui::Checkbox(strings[lang][Lang::SettingsImageViewFullscreenToggle], std::addressof(app.config.normal.enter_images_fullscreen)))
                Config::Save(app.config, app.fs);
            ImGui::PopID();
            
            EndAppletDisabled();

            Internal::Separator();

            // Developer Options
            Internal::Indent(strings[lang][Lang::SettingsDevOptsTitle]);

            // Logging is the only setting that works in applet mode (uses separate applet config)
            ImGui::PushID("dev_logs");
            if (GUI::IsAppletMode()) {
                if (ImGui::Checkbox(strings[lang][Lang::SettingsDevOptsLogsToggle], std::addressof(app.config.applet.dev_options)))
                    Config::Save(app.config, app.fs);
            } else {
                if (ImGui::Checkbox(strings[lang][Lang::SettingsDevOptsLogsToggle], std::addressof(app.config.normal.dev_options)))
                    Config::Save(app.config, app.fs);
            }
            ImGui::PopID();
            
            ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
            
            BeginAppletDisabled();
            ImGui::PushID("show_stats");
            if (ImGui::Checkbox(strings[lang][Lang::SettingsStatsToggle], std::addressof(app.config.normal.show_stats)))
                Config::Save(app.config, app.fs);
            ImGui::PopID();
            EndAppletDisabled();

            Internal::Separator();

            // Display Resolution
            BeginAppletDisabled();
            {
                // Build the title with current resolution indicator
                char resolution_title[128];
                std::snprintf(resolution_title, sizeof(resolution_title), "%s (%dp)", 
                    strings[lang][Lang::SettingsResolutionTitle], app.gui.display_height);
                Internal::Indent(resolution_title);

                if (ImGui::RadioButton(strings[lang][Lang::SettingsResolutionAuto], &app.config.normal.resolution_mode, ResolutionMode_Auto))
                    Config::Save(app.config, app.fs);
                
                ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
                
                if (ImGui::RadioButton(strings[lang][Lang::SettingsResolution1080p], &app.config.normal.resolution_mode, ResolutionMode_1080p))
                    Config::Save(app.config, app.fs);
                
                ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
                
                if (ImGui::RadioButton(strings[lang][Lang::SettingsResolution720p], &app.config.normal.resolution_mode, ResolutionMode_720p))
                    Config::Save(app.config, app.fs);
            }
            EndAppletDisabled();

            Internal::Separator();

            // Theme
            BeginAppletDisabled();
            {
                Internal::Indent(strings[lang][Lang::SettingsThemeTitle]);
                
                // Auto-size button width based on content for localization support
                const char* theme_labels[] = {
                    strings[lang][Lang::SettingsThemeAuto],
                    strings[lang][Lang::SettingsThemeDark],
                    strings[lang][Lang::SettingsThemeLight]
                };
                float max_text_width = 0.0f;
                for (int i = 0; i < 3; i++) {
                    float text_width = ImGui::CalcTextSize(theme_labels[i]).x;
                    if (text_width > max_text_width)
                        max_text_width = text_width;
                }
                
                // Button dimensions - auto-sized with padding
                const float button_width = max_text_width + 24.0f;  // Padding for button edges
                const float button_height = 36.0f;
                const float spacing = 8.0f;
                
                // Colors for theme buttons
                ImVec4 dark_bg = ImVec4(0.15f, 0.15f, 0.18f, 1.0f);
                ImVec4 dark_text = ImVec4(0.95f, 0.95f, 0.98f, 1.0f);
                ImVec4 light_bg = ImVec4(0.95f, 0.95f, 0.95f, 1.0f);
                ImVec4 light_text = ImVec4(0.10f, 0.10f, 0.12f, 1.0f);
                
                // Auto button uses the current Nintendo theme color
                bool system_is_dark = GUI::IsSystemThemeDark();
                ImVec4 auto_bg = system_is_dark ? dark_bg : light_bg;
                ImVec4 auto_text = system_is_dark ? dark_text : light_text;
                
                // Helper to draw themed button with selection indicator
                auto DrawThemeButton = [&](const char* label, int mode, ImVec4 bg_color, ImVec4 text_color) {
                    bool is_selected = (app.config.normal.theme_mode == mode);
                    
                    ImGui::PushID(mode);
                    
                    // Draw selection ring if selected
                    if (is_selected) {
                        ImVec2 cursor = ImGui::GetCursorScreenPos();
                        ImDrawList* draw_list = ImGui::GetWindowDrawList();
                        draw_list->AddRect(
                            ImVec2(cursor.x - 2, cursor.y - 2),
                            ImVec2(cursor.x + button_width + 2, cursor.y + button_height + 2),
                            GUI::GetAccentColorU32(app.config),
                            4.0f, 0, 2.0f
                        );
                    }
                    
                    ImGui::PushStyleColor(ImGuiCol_Button, bg_color);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(bg_color.x * 0.9f, bg_color.y * 0.9f, bg_color.z * 0.9f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(bg_color.x * 0.8f, bg_color.y * 0.8f, bg_color.z * 0.8f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_Text, text_color);
                    
                    if (ImGui::Button(label, ImVec2(button_width, button_height))) {
                        app.config.normal.theme_mode = mode;
                        Config::Save(app.config, app.fs);
                        GUI::UpdateThemeColors(app.config);
                        GUI::UpdateAccentColors(app.config);
                    }
                    
                    ImGui::PopStyleColor(4);
                    ImGui::PopID();
                };
                
                // Draw the three theme buttons in a row
                DrawThemeButton(strings[lang][Lang::SettingsThemeAuto], ThemeMode_Auto, auto_bg, auto_text);
                ImGui::SameLine(0, spacing);
                DrawThemeButton(strings[lang][Lang::SettingsThemeDark], ThemeMode_Dark, dark_bg, dark_text);
                ImGui::SameLine(0, spacing);
                DrawThemeButton(strings[lang][Lang::SettingsThemeLight], ThemeMode_Light, light_bg, light_text);
            }
            EndAppletDisabled();

            Internal::Separator();

            // Accent Color
            BeginAppletDisabled();
            {
                Internal::Indent(strings[lang][Lang::SettingsAccentColorTitle]);
                
                // Preset colors - 2 rows of 5, ordered starting from default teal
                static const ImVec4 preset_colors[9] = {
                    ImVec4(0.00f, 0.50f, 0.50f, 1.0f),  // Teal (default)
                    ImVec4(0.30f, 0.70f, 0.95f, 1.0f),  // Cyan
                    ImVec4(0.35f, 0.50f, 0.95f, 1.0f),  // Blue
                    ImVec4(0.65f, 0.40f, 0.85f, 1.0f),  // Purple
                    ImVec4(0.90f, 0.45f, 0.65f, 1.0f),  // Pink
                    ImVec4(0.90f, 0.25f, 0.25f, 1.0f),  // Red
                    ImVec4(0.95f, 0.55f, 0.20f, 1.0f),  // Orange
                    ImVec4(0.95f, 0.85f, 0.25f, 1.0f),  // Yellow
                    ImVec4(0.35f, 0.78f, 0.35f, 1.0f),  // Green
                };
                
                const float button_size = 36.0f;
                const float spacing = 8.0f;
                
                // Helper to check if current accent color matches a preset
                auto IsPresetSelected = [&app](const ImVec4& preset) {
                    const float epsilon = 0.01f;
                    return std::abs(app.config.normal.accent_color[0] - preset.x) < epsilon &&
                           std::abs(app.config.normal.accent_color[1] - preset.y) < epsilon &&
                           std::abs(app.config.normal.accent_color[2] - preset.z) < epsilon;
                };
                
                // Find which preset is selected (if any), or -1 for custom
                int selected_preset = -1;
                for (int i = 0; i < 9; i++) {
                    if (IsPresetSelected(preset_colors[i])) {
                        selected_preset = i;
                        break;
                    }
                }
                
                // Helper to draw a color preset button with selection indicator
                auto DrawColorPreset = [&](int index, const ImVec4& color) {
                    bool is_selected = (selected_preset == index);
                    
                    ImGui::PushID(index);
                    
                    // Draw selection ring if selected
                    if (is_selected) {
                        ImVec2 cursor = ImGui::GetCursorScreenPos();
                        ImDrawList* draw_list = ImGui::GetWindowDrawList();
                        // Use contrasting color for selection ring (theme-aware)
                        ImU32 ring_color = GUI::IsCurrentThemeDark(app.config) ? IM_COL32(255, 255, 255, 255) : IM_COL32(40, 40, 40, 255);
                        draw_list->AddRect(
                            ImVec2(cursor.x - 2, cursor.y - 2),
                            ImVec2(cursor.x + button_size + 2, cursor.y + button_size + 2),
                            ring_color,
                            4.0f, 0, 2.0f
                        );
                    }
                    
                    if (ImGui::ColorButton("##preset", color, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder, ImVec2(button_size, button_size))) {
                        app.config.normal.accent_color[0] = color.x;
                        app.config.normal.accent_color[1] = color.y;
                        app.config.normal.accent_color[2] = color.z;
                        Config::Save(app.config, app.fs);
                        GUI::UpdateAccentColors(app.config);
                    }
                    
                    ImGui::PopID();
                };
                
                // Row 1: First 5 presets
                for (int i = 0; i < 5; i++) {
                    if (i > 0) ImGui::SameLine(0, spacing);
                    DrawColorPreset(i, preset_colors[i]);
                }
                
                // Vertical spacing between rows
                ImGui::Dummy(ImVec2(0.0f, 2.0f));
                
                // Row 2: Next 4 presets + custom color button with settings icon
                for (int i = 5; i < 9; i++) {
                    if (i > 5) ImGui::SameLine(0, spacing);
                    DrawColorPreset(i, preset_colors[i]);
                }
                
                // Custom color button with settings icon
                ImGui::SameLine(0, spacing);
                ImVec4 custom_color = ImVec4(app.config.normal.accent_color[0], app.config.normal.accent_color[1], app.config.normal.accent_color[2], 1.0f);
                bool is_custom = (selected_preset == -1);
                
                // Draw selection ring if custom color is selected
                ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
                if (is_custom) {
                    ImDrawList* draw_list = ImGui::GetWindowDrawList();
                    ImU32 ring_color = GUI::IsCurrentThemeDark(app.config) ? IM_COL32(255, 255, 255, 255) : IM_COL32(40, 40, 40, 255);
                    draw_list->AddRect(
                        ImVec2(cursor_pos.x - 2, cursor_pos.y - 2),
                        ImVec2(cursor_pos.x + button_size + 2, cursor_pos.y + button_size + 2),
                        ring_color,
                        4.0f, 0, 2.0f
                    );
                }
                
                // Draw custom color button as background
                if (ImGui::ColorButton("##custom_bg", custom_color, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder, ImVec2(button_size, button_size))) {
                    ImGui::OpenPopup("##accent_picker");
                }
                
                // Draw settings icon centered on top of the button (tinted with accent color)
                const float icon_size = 20.0f;
                ImVec2 icon_pos = ImVec2(
                    cursor_pos.x + (button_size - icon_size) * 0.5f,
                    cursor_pos.y + (button_size - icon_size) * 0.5f
                );
                ImGui::GetWindowDrawList()->AddImage(
                    static_cast<ImTextureID>(app.textures.settings_icon.id),
                    icon_pos,
                    ImVec2(icon_pos.x + icon_size, icon_pos.y + icon_size),
                    ImVec2(0, 0), ImVec2(1, 1),
                    GUI::GetAccentColorU32(app.config)  // Tint with accent color like other icons
                );
                
                // Track popup state for initialization
                static bool picker_was_open = false;
                static float picker_h = 0.0f, picker_s = 1.0f, picker_v = 1.0f;
                
                // Reset HSV state when popup opens
                bool picker_is_open = ImGui::IsPopupOpen("##accent_picker");
                if (picker_is_open && !picker_was_open) {
                    // Popup just opened - initialize HSV from current RGB
                    ImGui::ColorConvertRGBtoHSV(app.config.normal.accent_color[0], app.config.normal.accent_color[1], app.config.normal.accent_color[2],
                                               picker_h, picker_s, picker_v);
                }
                picker_was_open = picker_is_open;
                
                // Suppress right stick scroll when color picker is open (we use it for hue control)
                GUI::SetRightStickScrollSuppressed(picker_is_open);
                
                // Color picker popup with gamepad controls
                if (ImGui::BeginPopup("##accent_picker")) {
                    
                    // Get analog stick input
                    float l_stick_x = 0.0f, l_stick_y = 0.0f;
                    float r_stick_x = 0.0f, r_stick_y = 0.0f;
                    ImGui_ImplSwitch_GetLeftStickPos(&l_stick_x, &l_stick_y);
                    ImGui_ImplSwitch_GetRightStickPos(&r_stick_x, &r_stick_y);
                    
                    // Dead zone for sticks
                    const float dead_zone = 0.15f;
                    const float stick_speed = 0.015f;
                    
                    bool color_changed = false;
                    
                    // L stick controls Saturation (X) and Value (Y)
                    if (std::abs(l_stick_x) > dead_zone) {
                        picker_s += l_stick_x * stick_speed;
                        picker_s = ImClamp(picker_s, 0.0f, 1.0f);
                        color_changed = true;
                    }
                    if (std::abs(l_stick_y) > dead_zone) {
                        picker_v += l_stick_y * stick_speed;
                        picker_v = ImClamp(picker_v, 0.0f, 1.0f);
                        color_changed = true;
                    }
                    
                    // R stick Y controls Hue (up/down)
                    if (std::abs(r_stick_y) > dead_zone) {
                        picker_h -= r_stick_y * stick_speed;
                        if (picker_h < 0.0f) picker_h += 1.0f;
                        if (picker_h > 1.0f) picker_h -= 1.0f;
                        color_changed = true;
                    }
                    
                    // Update RGB from HSV if changed via sticks
                    if (color_changed) {
                        ImGui::ColorConvertHSVtoRGB(picker_h, picker_s, picker_v,
                                                   app.config.normal.accent_color[0], app.config.normal.accent_color[1], app.config.normal.accent_color[2]);
                        Config::Save(app.config, app.fs);
                        GUI::UpdateAccentColors(app.config);
                    }
                    
                    // Draw the color picker (hide inputs/options)
                    ImGuiColorEditFlags picker_flags = 
                        ImGuiColorEditFlags_NoSidePreview | 
                        ImGuiColorEditFlags_NoSmallPreview |
                        ImGuiColorEditFlags_NoInputs |
                        ImGuiColorEditFlags_NoOptions |
                        ImGuiColorEditFlags_PickerHueBar;
                    
                    // Draw the picker itself
                    if (ImGui::ColorPicker3("##picker", app.config.normal.accent_color, picker_flags)) {
                        // Update HSV state when user clicks/touches
                        ImGui::ColorConvertRGBtoHSV(app.config.normal.accent_color[0], app.config.normal.accent_color[1], app.config.normal.accent_color[2],
                                                   picker_h, picker_s, picker_v);
                        Config::Save(app.config, app.fs);
                        GUI::UpdateAccentColors(app.config);
                    }
                    
                    // Get the picker's position to overlay our stick icons
                    // ImGui's color picker with PickerHueBar: SV square on left, hue bar on right
                    ImVec2 picker_pos = ImGui::GetItemRectMin();
                    ImVec2 picker_size = ImGui::GetItemRectSize();
                    ImDrawList* draw_list = ImGui::GetWindowDrawList();
                    
                    // Calculate positions for the SV square and hue bar
                    // Default picker is ~256px wide with ~20px hue bar on right
                    const float sv_size = picker_size.y;  // SV square is same height as picker
                    const float hue_bar_width = 20.0f;
                    
                    // Draw L stick icon in the SV field (bottom-left corner of SV area)
                    {
                        const float stick_radius = 10.0f;
                        const bool is_dark = GUI::IsCurrentThemeDark(app.config);
                        const ImU32 color_stick = is_dark ? IM_COL32(255, 255, 255, 200) : IM_COL32(40, 40, 40, 200);
                        const ImU32 color_bg = is_dark ? IM_COL32(45, 45, 45, 180) : IM_COL32(233, 233, 233, 180);
                        
                        ImVec2 stick_center(picker_pos.x + stick_radius + 8.0f,
                                           picker_pos.y + sv_size - stick_radius - 8.0f);
                        
                        // Draw background circle
                        draw_list->AddCircleFilled(stick_center, stick_radius + 2.0f, color_bg, 24);
                        draw_list->AddCircle(stick_center, stick_radius, color_stick, 24, 2.0f);
                        
                        // Draw X-shaped gaps
                        const float gap_width = 3.0f;
                        const float gap_length = stick_radius + 2.0f;
                        draw_list->AddRectFilled(
                            ImVec2(stick_center.x - gap_length, stick_center.y - gap_width * 0.5f),
                            ImVec2(stick_center.x + gap_length, stick_center.y + gap_width * 0.5f),
                            color_bg);
                        draw_list->AddRectFilled(
                            ImVec2(stick_center.x - gap_width * 0.5f, stick_center.y - gap_length),
                            ImVec2(stick_center.x + gap_width * 0.5f, stick_center.y + gap_length),
                            color_bg);
                        
                        // Redraw inner circle
                        draw_list->AddCircleFilled(stick_center, stick_radius - 3.0f, color_bg, 24);
                        draw_list->AddCircle(stick_center, stick_radius - 3.0f, color_stick, 24, 1.5f);
                        
                        // Draw "L" letter
                        const char* letter = "L";
                        const float font_size = ImGui::GetFontSize() * 0.65f;
                        ImVec2 text_size = ImGui::GetFont()->CalcTextSizeA(font_size, FLT_MAX, 0.0f, letter);
                        ImVec2 text_pos(stick_center.x - text_size.x * 0.5f, stick_center.y - text_size.y * 0.5f);
                        draw_list->AddText(ImGui::GetFont(), font_size, text_pos, color_stick, letter);
                    }
                    
                    // Draw R stick icon on the hue bar (centered vertically)
                    {
                        const float stick_radius = 10.0f;
                        const bool is_dark = GUI::IsCurrentThemeDark(app.config);
                        const ImU32 color_stick = is_dark ? IM_COL32(255, 255, 255, 200) : IM_COL32(40, 40, 40, 200);
                        const ImU32 color_bg = is_dark ? IM_COL32(45, 45, 45, 180) : IM_COL32(233, 233, 233, 180);
                        
                        // Position on the hue bar - right side of picker
                        ImVec2 stick_center(picker_pos.x + picker_size.x - hue_bar_width * 0.5f,
                                           picker_pos.y + sv_size * 0.5f);
                        
                        // Draw background circle
                        draw_list->AddCircleFilled(stick_center, stick_radius + 2.0f, color_bg, 24);
                        draw_list->AddCircle(stick_center, stick_radius, color_stick, 24, 2.0f);
                        
                        // Draw X-shaped gaps
                        const float gap_width = 3.0f;
                        const float gap_length = stick_radius + 2.0f;
                        draw_list->AddRectFilled(
                            ImVec2(stick_center.x - gap_length, stick_center.y - gap_width * 0.5f),
                            ImVec2(stick_center.x + gap_length, stick_center.y + gap_width * 0.5f),
                            color_bg);
                        draw_list->AddRectFilled(
                            ImVec2(stick_center.x - gap_width * 0.5f, stick_center.y - gap_length),
                            ImVec2(stick_center.x + gap_width * 0.5f, stick_center.y + gap_length),
                            color_bg);
                        
                        // Redraw inner circle
                        draw_list->AddCircleFilled(stick_center, stick_radius - 3.0f, color_bg, 24);
                        draw_list->AddCircle(stick_center, stick_radius - 3.0f, color_stick, 24, 1.5f);
                        
                        // Draw "R" letter
                        const char* letter = "R";
                        const float font_size = ImGui::GetFontSize() * 0.65f;
                        ImVec2 text_size = ImGui::GetFont()->CalcTextSizeA(font_size, FLT_MAX, 0.0f, letter);
                        ImVec2 text_pos(stick_center.x - text_size.x * 0.5f, stick_center.y - text_size.y * 0.5f);
                        draw_list->AddText(ImGui::GetFont(), font_size, text_pos, color_stick, letter);
                    }
                    
                    ImGui::Spacing();
                    
                    // Hex input field with (X) button indicator
                    {
                        // Convert current color to hex string
                        char hex_buf[8];
                        int r = static_cast<int>(app.config.normal.accent_color[0] * 255.0f + 0.5f);
                        int g = static_cast<int>(app.config.normal.accent_color[1] * 255.0f + 0.5f);
                        int b = static_cast<int>(app.config.normal.accent_color[2] * 255.0f + 0.5f);
                        std::snprintf(hex_buf, sizeof(hex_buf), "#%02X%02X%02X", r, g, b);
                        
                        // Color preview square
                        ImVec4 preview_col(app.config.normal.accent_color[0], app.config.normal.accent_color[1], app.config.normal.accent_color[2], 1.0f);
                        ImGui::ColorButton("##preview", preview_col, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder, ImVec2(24, 24));
                        ImGui::SameLine();
                        
                        // Hex text display
                        ImGui::AlignTextToFramePadding();
                        ImGui::Text("%s", hex_buf);
                        ImGui::SameLine();
                        
                        // Draw (X) button indicator for keyboard input
                        {
                            const float btn_radius = 10.0f;
                            ImU32 x_color = GUI::GetButtonColorX(app.config);
                            ImU32 text_color = GUI::GetButtonTextColor(app.config);
                            
                            ImVec2 btn_pos = ImGui::GetCursorScreenPos();
                            btn_pos.y += (ImGui::GetFrameHeight() - btn_radius * 2) * 0.5f;
                            ImVec2 btn_center(btn_pos.x + btn_radius, btn_pos.y + btn_radius);
                            
                            draw_list->AddCircleFilled(btn_center, btn_radius, x_color, 24);
                            
                            const char* letter = "X";
                            const float font_size = ImGui::GetFontSize() * 0.8f;
                            ImVec2 text_size = ImGui::GetFont()->CalcTextSizeA(font_size, FLT_MAX, 0.0f, letter);
                            ImVec2 text_pos(btn_center.x - text_size.x * 0.5f + 0.5f, btn_center.y - text_size.y * 0.5f);
                            draw_list->AddText(ImGui::GetFont(), font_size, text_pos, text_color, letter);
                            
                            // Reserve space for the button
                            ImGui::Dummy(ImVec2(btn_radius * 2 + 4, btn_radius * 2));
                        }
                        
                        // Handle X button press to open keyboard
                        if (ImGui_ImplSwitch_GetButtonsDown() & HidNpadButton_X) {
                            std::string result = Keyboard::GetText(app.config, "Enter hex color (e.g. #00FF00)", hex_buf);
                            if (!result.empty()) {
                                // Parse hex color
                                unsigned int hex_val = 0;
                                const char* parse_str = result.c_str();
                                if (parse_str[0] == '#') parse_str++;
                                if (std::sscanf(parse_str, "%06X", &hex_val) == 1) {
                                    app.config.normal.accent_color[0] = ((hex_val >> 16) & 0xFF) / 255.0f;
                                    app.config.normal.accent_color[1] = ((hex_val >> 8) & 0xFF) / 255.0f;
                                    app.config.normal.accent_color[2] = (hex_val & 0xFF) / 255.0f;
                                    // Update HSV state
                                    ImGui::ColorConvertRGBtoHSV(app.config.normal.accent_color[0], app.config.normal.accent_color[1], app.config.normal.accent_color[2],
                                                               picker_h, picker_s, picker_v);
                                    Config::Save(app.config, app.fs);
                                    GUI::UpdateAccentColors(app.config);
                                }
                            }
                        }
                    }
                    
                    ImGui::EndPopup();
                }
            }
            EndAppletDisabled();

            Internal::Separator();

            // Button Style
            BeginAppletDisabled();
            {
                Internal::Indent(strings[lang][Lang::SettingsButtonStyleTitle]);
                
                const float btn_radius = 12.0f;  // Match bottom bar size
                const float btn_spacing = 8.0f;
                const float buttons_offset = 162.0f;  // X offset from row start to button preview
                const bool is_dark = GUI::IsCurrentThemeDark(app.config);
                
                // Helper to draw button preview
                // Note: Colors here match GUI::GetButtonColor* functions for accurate preview
                // style: 0 = Colored, 1 = Mono, 2 = Accent
                auto DrawButtonPreview = [&](int style) {
                    ImU32 color_minus, color_a, color_b, color_x, color_y, color_plus;
                    ImU32 text_color;
                    ImU32 plusminus_color = is_dark ? IM_COL32(80, 80, 80, 255) : IM_COL32(140, 140, 145, 255);
                    
                    if (style == ButtonStyle_Colored) {
                        // Colored style: Nintendo Switch standard colors
                        color_minus = plusminus_color;
                        color_a = IM_COL32(235, 64, 52, 255);    // Red
                        color_b = IM_COL32(200, 150, 0, 255);    // Yellow
                        color_x = IM_COL32(65, 137, 230, 255);   // Blue
                        color_y = IM_COL32(100, 180, 100, 255);  // Green
                        color_plus = plusminus_color;
                        text_color = IM_COL32(255, 255, 255, 255);  // White text
                    } else if (style == ButtonStyle_Mono) {
                        // Mono style: Theme-aware gray buttons
                        ImU32 mono = is_dark ? IM_COL32(180, 180, 180, 255) : IM_COL32(80, 80, 80, 255);
                        color_minus = color_plus = plusminus_color;
                        color_a = color_b = color_x = color_y = mono;
                        // Contrasting text color for mono buttons
                        text_color = is_dark ? IM_COL32(40, 40, 40, 255) : IM_COL32(240, 240, 240, 255);
                    } else {
                        // Accent style: All ABXY buttons use accent color
                        ImU32 accent = GUI::GetAccentColorU32(app.config);
                        color_minus = color_plus = plusminus_color;
                        color_a = color_b = color_x = color_y = accent;
                        // Calculate luminance for text color
                        float luminance = 0.299f * app.config.normal.accent_color[0] + 0.587f * app.config.normal.accent_color[1] + 0.114f * app.config.normal.accent_color[2];
                        text_color = luminance > 0.5f ? IM_COL32(30, 30, 30, 255) : IM_COL32(255, 255, 255, 255);
                    }
                    ImDrawList* draw_list = ImGui::GetWindowDrawList();
                    ImVec2 cursor = ImGui::GetCursorScreenPos();
                    float x = cursor.x;
                    float center_y = cursor.y + btn_radius;
                    
                    // Draw minus button (-)
                    {
                        ImVec2 center(x + btn_radius, center_y);
                        draw_list->AddCircleFilled(center, btn_radius, color_minus, 24);
                        const float minus_width = btn_radius * 0.7f;
                        draw_list->AddLine(
                            ImVec2(center.x - minus_width, center.y),
                            ImVec2(center.x + minus_width, center.y),
                            text_color, 2.0f
                        );
                        x += btn_radius * 2 + btn_spacing;
                    }
                    
                    // Draw A, B, X, Y buttons
                    const char* letters[] = { "A", "B", "X", "Y" };
                    ImU32 colors[] = { color_a, color_b, color_x, color_y };
                    for (int i = 0; i < 4; i++) {
                        ImVec2 center(x + btn_radius, center_y);
                        draw_list->AddCircleFilled(center, btn_radius, colors[i], 24);
                        ImVec2 text_size = ImGui::CalcTextSize(letters[i]);
                        draw_list->AddText(ImVec2(center.x - text_size.x * 0.5f + 0.5f, center.y - text_size.y * 0.5f), text_color, letters[i]);
                        x += btn_radius * 2 + btn_spacing;
                    }
                    
                    // Draw plus button (+)
                    {
                        ImVec2 center(x + btn_radius, center_y);
                        draw_list->AddCircleFilled(center, btn_radius, color_plus, 24);
                        const float plus_width = btn_radius * 0.7f;
                        draw_list->AddLine(
                            ImVec2(center.x - plus_width, center.y),
                            ImVec2(center.x + plus_width, center.y),
                            text_color, 2.0f
                        );
                        draw_list->AddLine(
                            ImVec2(center.x, center.y - plus_width),
                            ImVec2(center.x, center.y + plus_width),
                            text_color, 2.0f
                        );
                        x += btn_radius * 2;
                    }
                    
                    // Reserve space
                    ImGui::Dummy(ImVec2(x - cursor.x, btn_radius * 2));
                };
                
                // Calculate total width for invisible hit area
                const float total_buttons_width = (btn_radius * 2 + btn_spacing) * 5 + btn_radius * 2;
                
                // Colored option - standard RadioButton + button preview
                {
                    float row_start_x = ImGui::GetCursorPosX();
                    ImVec2 row_start_screen = ImGui::GetCursorScreenPos();
                    
                    // Draw the standard radio button with label
                    if (ImGui::RadioButton(strings[lang][Lang::SettingsButtonStyleColored], &app.config.normal.button_style, ButtonStyle_Colored)) {
                        Config::Save(app.config, app.fs);
                    }
                    float row_height = ImGui::GetItemRectSize().y;
                    
                    // Draw button preview on the same line at fixed position
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(row_start_x + buttons_offset);
                    // Vertically center the button preview with the radio button
                    float preview_y = ImGui::GetCursorPosY() + (row_height - btn_radius * 2) * 0.5f;
                    ImGui::SetCursorPosY(preview_y);
                    DrawButtonPreview(ButtonStyle_Colored);
                    
                    // Add invisible button over the button preview area to extend click area
                    ImGui::SetCursorScreenPos(ImVec2(row_start_screen.x + buttons_offset, row_start_screen.y));
                    if (ImGui::InvisibleButton("##colored_ext", ImVec2(total_buttons_width, row_height))) {
                        app.config.normal.button_style = ButtonStyle_Colored;
                        Config::Save(app.config, app.fs);
                    }
                }
                
                ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
                
                // Mono option - standard RadioButton + button preview
                {
                    float row_start_x = ImGui::GetCursorPosX();
                    ImVec2 row_start_screen = ImGui::GetCursorScreenPos();
                    
                    // Draw the standard radio button with label
                    if (ImGui::RadioButton(strings[lang][Lang::SettingsButtonStyleMono], &app.config.normal.button_style, ButtonStyle_Mono)) {
                        Config::Save(app.config, app.fs);
                    }
                    float row_height = ImGui::GetItemRectSize().y;
                    
                    // Draw button preview on the same line at fixed position
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(row_start_x + buttons_offset);
                    // Vertically center the button preview with the radio button
                    float preview_y = ImGui::GetCursorPosY() + (row_height - btn_radius * 2) * 0.5f;
                    ImGui::SetCursorPosY(preview_y);
                    DrawButtonPreview(ButtonStyle_Mono);
                    
                    // Add invisible button over the button preview area to extend click area
                    ImGui::SetCursorScreenPos(ImVec2(row_start_screen.x + buttons_offset, row_start_screen.y));
                    if (ImGui::InvisibleButton("##mono_ext", ImVec2(total_buttons_width, row_height))) {
                        app.config.normal.button_style = ButtonStyle_Mono;
                        Config::Save(app.config, app.fs);
                    }
                }
                
                ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
                
                // Accent option - standard RadioButton + button preview
                {
                    float row_start_x = ImGui::GetCursorPosX();
                    ImVec2 row_start_screen = ImGui::GetCursorScreenPos();
                    
                    // Draw the standard radio button with label
                    if (ImGui::RadioButton(strings[lang][Lang::SettingsButtonStyleAccent], &app.config.normal.button_style, ButtonStyle_Accent)) {
                        Config::Save(app.config, app.fs);
                    }
                    float row_height = ImGui::GetItemRectSize().y;
                    
                    // Draw button preview on the same line at fixed position
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(row_start_x + buttons_offset);
                    // Vertically center the button preview with the radio button
                    float preview_y = ImGui::GetCursorPosY() + (row_height - btn_radius * 2) * 0.5f;
                    ImGui::SetCursorPosY(preview_y);
                    DrawButtonPreview(ButtonStyle_Accent);
                    
                    // Add invisible button over the button preview area to extend click area
                    ImGui::SetCursorScreenPos(ImVec2(row_start_screen.x + buttons_offset, row_start_screen.y));
                    if (ImGui::InvisibleButton("##accent_ext", ImVec2(total_buttons_width, row_height))) {
                        app.config.normal.button_style = ButtonStyle_Accent;
                        Config::Save(app.config, app.fs);
                    }
                }
            }
            EndAppletDisabled();

            Internal::Separator();

            // Reset All Settings
            BeginAppletDisabled();
            {
                Internal::Indent(strings[lang][Lang::SettingsResetTitle]);
                
                const char* reset_text = strings[lang][Lang::SettingsResetButton];
                float reset_button_width = std::max(250.0f, ImGui::CalcTextSize(reset_text).x + 40.0f);
                if (ImGui::Button(reset_text, ImVec2(reset_button_width, 40)))
                    reset_settings_popup = true;
                
                ImGui::Dummy(ImVec2(0.0f, 15.0f)); // Whitespace below button
            }
            EndAppletDisabled();
            
            Internal::EndSection();
            
            ImGui::EndChild();
            
            // Draw bottom bar with (-) Quit, (A) Select, and (B) Back buttons
            Internal::DrawBottomBar(app.config, {Internal::Button::Quit}, {Internal::Button::Select, Internal::Button::Back});

            ImGui::EndTabItem();
        }

        if (unmount_popup)
            Popups::USBPopup(app, unmount_popup);

        // Reset Settings Confirmation Popup
        if (reset_settings_popup) {
            const int lang = app.config.Lang();
            Popups::SetupPopup(app, strings[lang][Lang::SettingsResetTitle]);
            
            if (ImGui::BeginPopupModal(strings[lang][Lang::SettingsResetTitle], nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("%s", strings[lang][Lang::SettingsResetMessage]);
                
                ImGui::Dummy(ImVec2(0.0f, 10.0f)); // Spacing
                
                if (ImGui::Button(strings[lang][Lang::HintConfirm], ImVec2(120, 0))) {
                    // Reset all settings to defaults
                    Config::ResetNormalConfig(app.config);
                    Config::Save(app.config, app.fs);
                    
                    // Reset ImGui settings (table column widths, etc.)
                    GUI::ResetImGuiSettings(app.fs);
                    
                    // Apply the reset theme and accent colors
                    GUI::UpdateThemeColors(app.config);
                    GUI::UpdateAccentColors(app.config);
                    GUI::ResetUIState(app);
                    
                    ImGui::CloseCurrentPopup();
                    reset_settings_popup = false;
                }
                
                ImGui::SameLine(0.0f, 15.0f);
                
                if (ImGui::Button(strings[lang][Lang::HintCancel], ImVec2(120, 0))) {
                    ImGui::CloseCurrentPopup();
                    reset_settings_popup = false;
                }
                
                Popups::ExitPopup();
            }
            else {
                // Popup was closed (e.g., by B button) - reset the flag
                ImGui::PopStyleVar();
                reset_settings_popup = false;
            }
        }
    }
}
