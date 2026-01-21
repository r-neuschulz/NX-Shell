#include "config.hpp"
#include "fs.hpp"
#include "gui.hpp"
#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"
#include "language.hpp"
#include "log.hpp"
#include "net.hpp"
#include "popups.hpp"
#include "tabs.hpp"
#include "tabs_internal.hpp"
#include "textures.hpp"
#include "usb.hpp"

static bool need_focus_settings = false;

namespace Tabs {
    void RequestSettingsFocus(void) {
        need_focus_settings = true;
    }

    static bool unmount_popup = false;
    static bool reset_settings_popup = false;
    
    void Settings(WindowData &data, int &current_tab, int &active_tab) {
        ImGuiTabItemFlags flags = (current_tab == 1) ? ImGuiTabItemFlags_SetSelected : 0;
        if (current_tab == 1) current_tab = -1; // Reset after applying
        
        if (ImGui::BeginTabItem(strings[Config::GetLang()][Lang::TabSettings], nullptr, flags)) {
            active_tab = 1;  // Update active tab when this tab is visible
            const int lang = Config::GetLang();
            
            // Reserve space for button bar at the bottom
            const float button_bar_height = 40.0f;
            float available_height = ImGui::GetContentRegionAvail().y - button_bar_height;
            
            // Begin scrollable child region for settings content
            ImGui::BeginChild("SettingsContent", ImVec2(0, available_height), false);
            
            // Language selector
            ImGui::Indent(10.f);
            Internal::Indent(strings[lang][Lang::SettingsLanguageTitle]);

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
                if (supported_languages[i].value == cfg.lang) {
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
                                   cfg.lang, supported_languages[i].value, cfg.show_stats);
                        cfg.lang = supported_languages[i].value;
                        Config::Save(cfg);
                        Log::Debug("After Config::Save: show_stats=%d\n", cfg.show_stats);
                        GUI::ResetUIState();  // Force full UI refresh for new language
                        Log::Debug("After ResetUIState: show_stats=%d\n", cfg.show_stats);
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

            Internal::Separator();

            // USB unmount
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

            Internal::Separator();

            // Image viewer options
            Internal::Indent(strings[lang][Lang::SettingsImageViewTitle]);

            ImGui::PushID("image_filename");
            if (ImGui::Checkbox(strings[lang][Lang::SettingsImageViewFilenameToggle], std::addressof(cfg.image_filename)))
                Config::Save(cfg);
            ImGui::PopID();

            ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
            
            ImGui::PushID("enter_images_fullscreen");
            if (ImGui::Checkbox(strings[lang][Lang::SettingsImageViewFullscreenToggle], std::addressof(cfg.enter_images_fullscreen)))
                Config::Save(cfg);
            ImGui::PopID();

            Internal::Separator();

            // Developer Options
            Internal::Indent(strings[lang][Lang::SettingsDevOptsTitle]);

            ImGui::PushID("dev_logs");
            if (ImGui::Checkbox(strings[lang][Lang::SettingsDevOptsLogsToggle], std::addressof(cfg.dev_options)))
                Config::Save(cfg);
            ImGui::PopID();
            
            ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
            
            ImGui::PushID("show_stats");
            if (ImGui::Checkbox(strings[lang][Lang::SettingsStatsToggle], std::addressof(cfg.show_stats)))
                Config::Save(cfg);
            ImGui::PopID();

            Internal::Separator();

            // Display Resolution
            {
                // Build the title with current resolution indicator
                char resolution_title[128];
                std::snprintf(resolution_title, sizeof(resolution_title), "%s (%dp)", 
                    strings[lang][Lang::SettingsResolutionTitle], GUI::display_height);
                Internal::Indent(resolution_title);

                if (ImGui::RadioButton(strings[lang][Lang::SettingsResolutionAuto], &cfg.resolution_mode, ResolutionMode_Auto))
                    Config::Save(cfg);
                
                ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
                
                if (ImGui::RadioButton(strings[lang][Lang::SettingsResolution1080p], &cfg.resolution_mode, ResolutionMode_1080p))
                    Config::Save(cfg);
                
                ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
                
                if (ImGui::RadioButton(strings[lang][Lang::SettingsResolution720p], &cfg.resolution_mode, ResolutionMode_720p))
                    Config::Save(cfg);
            }

            Internal::Separator();

            // Theme
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
                    bool is_selected = (cfg.theme_mode == mode);
                    
                    ImGui::PushID(mode);
                    
                    // Draw selection ring if selected
                    if (is_selected) {
                        ImVec2 cursor = ImGui::GetCursorScreenPos();
                        ImDrawList* draw_list = ImGui::GetWindowDrawList();
                        draw_list->AddRect(
                            ImVec2(cursor.x - 2, cursor.y - 2),
                            ImVec2(cursor.x + button_width + 2, cursor.y + button_height + 2),
                            GUI::GetAccentColorU32(),
                            4.0f, 0, 2.0f
                        );
                    }
                    
                    ImGui::PushStyleColor(ImGuiCol_Button, bg_color);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(bg_color.x * 0.9f, bg_color.y * 0.9f, bg_color.z * 0.9f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(bg_color.x * 0.8f, bg_color.y * 0.8f, bg_color.z * 0.8f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_Text, text_color);
                    
                    if (ImGui::Button(label, ImVec2(button_width, button_height))) {
                        cfg.theme_mode = mode;
                        Config::Save(cfg);
                        GUI::UpdateThemeColors();
                        GUI::UpdateAccentColors();
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

            Internal::Separator();

            // Accent Color
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
                auto IsPresetSelected = [](const ImVec4& preset) {
                    const float epsilon = 0.01f;
                    return std::abs(cfg.accent_color[0] - preset.x) < epsilon &&
                           std::abs(cfg.accent_color[1] - preset.y) < epsilon &&
                           std::abs(cfg.accent_color[2] - preset.z) < epsilon;
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
                        ImU32 ring_color = GUI::IsCurrentThemeDark() ? IM_COL32(255, 255, 255, 255) : IM_COL32(40, 40, 40, 255);
                        draw_list->AddRect(
                            ImVec2(cursor.x - 2, cursor.y - 2),
                            ImVec2(cursor.x + button_size + 2, cursor.y + button_size + 2),
                            ring_color,
                            4.0f, 0, 2.0f
                        );
                    }
                    
                    if (ImGui::ColorButton("##preset", color, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder, ImVec2(button_size, button_size))) {
                        cfg.accent_color[0] = color.x;
                        cfg.accent_color[1] = color.y;
                        cfg.accent_color[2] = color.z;
                        Config::Save(cfg);
                        GUI::UpdateAccentColors();
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
                ImVec4 custom_color = ImVec4(cfg.accent_color[0], cfg.accent_color[1], cfg.accent_color[2], 1.0f);
                bool is_custom = (selected_preset == -1);
                
                // Draw selection ring if custom color is selected
                ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
                if (is_custom) {
                    ImDrawList* draw_list = ImGui::GetWindowDrawList();
                    ImU32 ring_color = GUI::IsCurrentThemeDark() ? IM_COL32(255, 255, 255, 255) : IM_COL32(40, 40, 40, 255);
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
                    static_cast<ImTextureID>(settings_icon.id),
                    icon_pos,
                    ImVec2(icon_pos.x + icon_size, icon_pos.y + icon_size),
                    ImVec2(0, 0), ImVec2(1, 1),
                    GUI::GetAccentColorU32()  // Tint with accent color like other icons
                );
                
                // Color picker popup
                if (ImGui::BeginPopup("##accent_picker")) {
                    if (ImGui::ColorPicker3("##picker", cfg.accent_color, ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview)) {
                        Config::Save(cfg);
                        GUI::UpdateAccentColors();
                    }
                    ImGui::EndPopup();
                }
            }

            Internal::Separator();

            // Button Style
            {
                Internal::Indent(strings[lang][Lang::SettingsButtonStyleTitle]);
                
                const float btn_radius = 12.0f;  // Match bottom bar size
                const float btn_spacing = 8.0f;
                const float buttons_offset = 130.0f;  // X offset from row start to button preview
                const bool is_dark = GUI::IsCurrentThemeDark();
                
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
                        ImU32 accent = GUI::GetAccentColorU32();
                        color_minus = color_plus = plusminus_color;
                        color_a = color_b = color_x = color_y = accent;
                        // Calculate luminance for text color
                        float luminance = 0.299f * cfg.accent_color[0] + 0.587f * cfg.accent_color[1] + 0.114f * cfg.accent_color[2];
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
                    if (ImGui::RadioButton(strings[lang][Lang::SettingsButtonStyleColored], &cfg.button_style, ButtonStyle_Colored)) {
                        Config::Save(cfg);
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
                        cfg.button_style = ButtonStyle_Colored;
                        Config::Save(cfg);
                    }
                }
                
                ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
                
                // Mono option - standard RadioButton + button preview
                {
                    float row_start_x = ImGui::GetCursorPosX();
                    ImVec2 row_start_screen = ImGui::GetCursorScreenPos();
                    
                    // Draw the standard radio button with label
                    if (ImGui::RadioButton(strings[lang][Lang::SettingsButtonStyleMono], &cfg.button_style, ButtonStyle_Mono)) {
                        Config::Save(cfg);
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
                        cfg.button_style = ButtonStyle_Mono;
                        Config::Save(cfg);
                    }
                }
                
                ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
                
                // Accent option - standard RadioButton + button preview
                {
                    float row_start_x = ImGui::GetCursorPosX();
                    ImVec2 row_start_screen = ImGui::GetCursorScreenPos();
                    
                    // Draw the standard radio button with label
                    if (ImGui::RadioButton(strings[lang][Lang::SettingsButtonStyleAccent], &cfg.button_style, ButtonStyle_Accent)) {
                        Config::Save(cfg);
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
                        cfg.button_style = ButtonStyle_Accent;
                        Config::Save(cfg);
                    }
                }
            }

            Internal::Separator();

            // Reset All Settings
            {
                Internal::Indent(strings[lang][Lang::SettingsResetTitle]);
                
                const char* reset_text = strings[lang][Lang::SettingsResetButton];
                float reset_button_width = std::max(250.0f, ImGui::CalcTextSize(reset_text).x + 40.0f);
                if (ImGui::Button(reset_text, ImVec2(reset_button_width, 40)))
                    reset_settings_popup = true;
            }
            
            ImGui::EndChild();
            
            // Draw bottom bar with (-) Quit, (A) Select, and (B) Back buttons
            Internal::DrawBottomBar({Internal::Button::Quit}, {Internal::Button::Select, Internal::Button::Back});

            ImGui::EndTabItem();
        }

        if (unmount_popup)
            Popups::USBPopup(unmount_popup);

        // Reset Settings Confirmation Popup
        if (reset_settings_popup) {
            const int lang = Config::GetLang();
            Popups::SetupPopup(strings[lang][Lang::SettingsResetTitle]);
            
            if (ImGui::BeginPopupModal(strings[lang][Lang::SettingsResetTitle], nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("%s", strings[lang][Lang::SettingsResetMessage]);
                
                ImGui::Dummy(ImVec2(0.0f, 10.0f)); // Spacing
                
                if (ImGui::Button(strings[lang][Lang::HintConfirm], ImVec2(120, 0))) {
                    // Reset all settings to defaults
                    cfg = config_t{};
                    Config::Save(cfg);
                    
                    // Reset ImGui settings (table column widths, etc.)
                    GUI::ResetImGuiSettings();
                    
                    // Apply the reset theme and accent colors
                    GUI::UpdateThemeColors();
                    GUI::UpdateAccentColors();
                    GUI::ResetUIState();
                    
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
