#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "bottombar.hpp"
#include "config.hpp"
#include "fs.hpp"
#include "services.hpp"
#include "gui.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "language.hpp"
#include "popups.hpp"
#include "windows.hpp"

namespace TextReader {
    // Maximum file size to load (5 MB to prevent memory issues)
    static constexpr std::size_t MAX_FILE_SIZE = 5 * 1024 * 1024;
    
    // Scroll state managed internally
    static float s_scroll_y = 0.0f;
    static float s_max_scroll_y = 0.0f;
    static float s_saved_scroll_offset = 0.0f;  // Saved offset for next/prev file navigation
    
    // Scroll acceleration state
    static float s_scroll_hold_time = 0.0f;
    static constexpr float SCROLL_ACCEL_RAMP_TIME = 1.0f;  // Time in seconds to reach max speed
    
    // Hex mode state
    static bool s_hex_mode = false;
    static std::vector<unsigned char> s_raw_content;  // Raw binary content for hex view
    
    // Hex mode constants
    static constexpr int BYTES_PER_LINE = 16;
    
    bool IsHexMode(void) { return s_hex_mode; }
    void SetHexMode(bool mode) { s_hex_mode = mode; }
    void ToggleHexMode(void) { s_hex_mode = !s_hex_mode; }
    
    bool LoadFile(App &app, const std::string &path, bool restore_offset) {
        FILE *file = fopen(path.c_str(), "rb");
        if (!file) {
            return false;
        }
        
        // Get file size
        fseek(file, 0, SEEK_END);
        std::size_t size = ftell(file);
        fseek(file, 0, SEEK_SET);
        
        // Check file size limit
        if (size > MAX_FILE_SIZE) {
            fclose(file);
            app.window.text_content = "[File too large to display. Maximum size: 5 MB]";
            s_raw_content.clear();
            return true;  // Still open viewer with message
        }
        
        // Read file content as raw bytes
        s_raw_content.resize(size);
        std::size_t read = fread(s_raw_content.data(), 1, size, file);
        fclose(file);
        
        if (read != size) {
            app.window.text_content = "[Error reading file]";
            s_raw_content.clear();
            return true;
        }
        
        // Store content as string for text mode
        app.window.text_content.assign(reinterpret_cast<char*>(s_raw_content.data()), size);
        
        // Restore saved offset when navigating between files, otherwise reset to 0
        if (restore_offset) {
            s_scroll_y = s_saved_scroll_offset;
        } else {
            s_scroll_y = 0.0f;
        }
        s_max_scroll_y = 0.0f;
        return true;
    }
    
    void SaveScrollOffset(void) {
        s_saved_scroll_offset = s_scroll_y;
    }
    
    void Clear(App &app) {
        app.window.text_content.clear();
        s_raw_content.clear();
        s_scroll_y = 0.0f;
        s_max_scroll_y = 0.0f;
        s_hex_mode = false;
    }
    
    bool HandleScroll(App &app, int index, bool restore_offset) {
        if (app.window.entries[index].type == FsDirEntryType_Dir)
            return false;
        
        // Check if it's a text or binary file (both can be opened in text reader)
        FileType type = FS::GetFileType(app.window.entries[index].name);
        if (type != FileTypeText && type != FileTypeBinary)
            return false;
        
        // Auto-enable hex mode for binary files
        s_hex_mode = (type == FileTypeBinary);
        
        app.window.selected = index;
        std::string path = FS::BuildPath(app.fs, app.window.entries[index]);
        return TextReader::LoadFile(app, path, restore_offset);
    }
    
    // Navigate to adjacent text file (direction: -1 = prev, +1 = next)
    static bool HandleNavigate(App &app, int direction) {
        const int count = static_cast<int>(app.window.entries.size());
        const int start = static_cast<int>(app.window.selected) + direction;
        
        // Bounds check
        if (direction > 0 && app.window.selected >= static_cast<u64>(count))
            return false;
        
        // Save current offset before navigating
        TextReader::SaveScrollOffset();
        
        // Search in direction from current position
        for (int i = start; direction > 0 ? i < count : i >= 0; i += direction) {
            if (TextReader::HandleScroll(app, i, true))  // restore_offset = true
                return true;
        }
        
        // Wrap around: search from opposite end
        int wrap_start = direction > 0 ? 0 : count - 1;
        int wrap_end = static_cast<int>(app.window.selected);
        for (int i = wrap_start; direction > 0 ? i < wrap_end : i > wrap_end; i += direction) {
            if (TextReader::HandleScroll(app, i, true))  // restore_offset = true
                return true;
        }
        
        return false;  // No other text files found
    }

    // Public API wrappers
    bool HandlePrev(App &app) { return HandleNavigate(app, -1); }
    bool HandleNext(App &app) { return HandleNavigate(app, +1); }
    
    void HandleControls(App &app, u64 &key, bool &properties) {
        if (key & HidNpadButton_X)
            properties = true;
        if (!properties) {
            if (key & HidNpadButton_B) {
                TextReader::Clear(app);
                app.window.state = WINDOW_STATE_FILEBROWSER;
            }
            
            // Y button toggles hex mode
            if (key & HidNpadButton_Y) {
                TextReader::ToggleHexMode();
                s_scroll_y = 0.0f;  // Reset scroll when switching modes
            }
            
            if (key & HidNpadButton_L) {
                TextReader::Clear(app);
                
                if (!TextReader::HandlePrev(app))
                    app.window.state = WINDOW_STATE_FILEBROWSER;
            }
            else if (key & HidNpadButton_R) {
                TextReader::Clear(app);
                
                if (!TextReader::HandleNext(app))
                    app.window.state = WINDOW_STATE_FILEBROWSER;
            }
            
            // Scroll speed constants
            const float dpad_base_speed = 50.0f;
            const float dpad_max_speed = 500.0f;       // 10x base speed for dpad
            const float stick_base_speed = 120.0f;
            const float stick_max_speed = 1200.0f;     // 10x base speed for stick
            const float dt = 1.0f / 60.0f;             // Approximate frame time
            
            // Check if any scroll input is active
            bool scroll_active = (key & (HidNpadButton_Up | HidNpadButton_Down | 
                                         HidNpadButton_StickRUp | HidNpadButton_StickRDown)) != 0;
            
            if (scroll_active) {
                // Accumulate hold time for acceleration
                s_scroll_hold_time += dt;
                if (s_scroll_hold_time > SCROLL_ACCEL_RAMP_TIME)
                    s_scroll_hold_time = SCROLL_ACCEL_RAMP_TIME;
            } else {
                // Reset hold time when not scrolling
                s_scroll_hold_time = 0.0f;
            }
            
            // Calculate acceleration factor (0.0 to 1.0 based on hold time)
            float accel_factor = s_scroll_hold_time / SCROLL_ACCEL_RAMP_TIME;
            // Use a smooth easing curve for natural feel
            accel_factor = accel_factor * accel_factor;  // Quadratic ease-in
            
            // Calculate current scroll speeds with acceleration
            float dpad_speed = dpad_base_speed + (dpad_max_speed - dpad_base_speed) * accel_factor;
            float stick_speed = stick_base_speed + (stick_max_speed - stick_base_speed) * accel_factor;
            
            // DPad scrolling (accelerates from base to max speed)
            if (key & HidNpadButton_Up) {
                s_scroll_y -= dpad_speed;
                if (s_scroll_y < 0.0f) s_scroll_y = 0.0f;
            }
            if (key & HidNpadButton_Down) {
                s_scroll_y += dpad_speed;
                if (s_scroll_y > s_max_scroll_y) s_scroll_y = s_max_scroll_y;
            }
            
            // Right stick scrolling (accelerates from base to max speed)
            if (key & HidNpadButton_StickRUp) {
                s_scroll_y -= stick_speed;
                if (s_scroll_y < 0.0f) s_scroll_y = 0.0f;
            }
            if (key & HidNpadButton_StickRDown) {
                s_scroll_y += stick_speed;
                if (s_scroll_y > s_max_scroll_y) s_scroll_y = s_max_scroll_y;
            }
        }
    }
    
    float GetScrollY(void) { return s_scroll_y; }
    void SetScrollY(float y) { s_scroll_y = y; }
    void SetMaxScrollY(float max_y) { s_max_scroll_y = max_y; }
    
    const std::vector<unsigned char>& GetRawContent(void) { return s_raw_content; }
}

namespace Windows {
    static void DrawTextReaderBottomBar(App &app) {
        const int lang = app.config.Lang();
        
        // Left-aligned items (minus button for exit)
        std::vector<BottomBar::HintItem> left_items = {
            {BottomBar::ButtonType::Minus, strings[lang][Lang::HintExit]}
        };
        
        // Right-aligned items
        std::vector<BottomBar::HintItem> right_items = {
            {BottomBar::ButtonType::CircleB, strings[lang][Lang::HintBack]},
            {BottomBar::ButtonType::CircleY, strings[lang][Lang::HintHexMode], TextReader::IsHexMode()},
            {BottomBar::ButtonType::CircleX, strings[lang][Lang::HintProperties]},
            {BottomBar::ButtonType::ShoulderL, strings[lang][Lang::HintPrev]},
            {BottomBar::ButtonType::ShoulderR, strings[lang][Lang::HintNext]},
            {BottomBar::ButtonType::RightStick, "Scroll"}
        };
        
        BottomBar::Config config;
        config.use_foreground_draw_list = true;
        config.draw_background = true;
        
        BottomBar::Draw(app.config, config, left_items, right_items);
    }
    
    // Helper function to render hex view with virtual scrolling for performance
    static void RenderHexView(ConfigService &config_svc) {
        const auto& raw_content = TextReader::GetRawContent();
        if (raw_content.empty()) {
            ImGui::TextUnformatted("[Empty file]");
            return;
        }
        
        const bool is_dark = GUI::IsCurrentThemeDark(config_svc);
        const ImU32 offset_color = is_dark ? IM_COL32(100, 180, 255, 255) : IM_COL32(0, 100, 200, 255);  // Blue for offset
        const ImU32 hex_color = is_dark ? IM_COL32(220, 220, 220, 255) : IM_COL32(30, 30, 30, 255);      // Main hex color
        const ImU32 ascii_color = is_dark ? IM_COL32(180, 255, 180, 255) : IM_COL32(0, 130, 0, 255);     // Green for ASCII
        const ImU32 non_print_color = is_dark ? IM_COL32(100, 100, 100, 255) : IM_COL32(180, 180, 180, 255);  // Gray for non-printable
        
        constexpr int BYTES_PER_LINE = 16;
        const int total_lines = static_cast<int>((raw_content.size() + BYTES_PER_LINE - 1) / BYTES_PER_LINE);
        
        // Pre-calculate character widths for alignment
        const float char_width = ImGui::CalcTextSize("0").x;
        const float ascii_char_spacing = char_width * 1.4f;  // Wider spacing for ASCII section readability
        const float offset_width = char_width * 10;  // "00000000: "
        const float hex_section_width = char_width * (BYTES_PER_LINE * 3 + 2);  // "XX " * 16 + extra spacing
        const float line_height = ImGui::GetTextLineHeight();
        const float total_line_width = offset_width + hex_section_width + ascii_char_spacing * BYTES_PER_LINE + char_width * 4;
        
        char line_buf[256];
        
        // Use ImGuiListClipper for virtual scrolling - only renders visible lines
        ImGuiListClipper clipper;
        clipper.Begin(total_lines, line_height);
        
        while (clipper.Step()) {
            for (int line = clipper.DisplayStart; line < clipper.DisplayEnd; line++) {
                const std::size_t offset = static_cast<std::size_t>(line) * BYTES_PER_LINE;
                const std::size_t bytes_in_line = std::min(static_cast<std::size_t>(BYTES_PER_LINE), raw_content.size() - offset);
                
                ImVec2 pos = ImGui::GetCursorScreenPos();
                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                
                // Draw offset (address)
                std::snprintf(line_buf, sizeof(line_buf), "%08zX: ", offset);
                draw_list->AddText(pos, offset_color, line_buf);
                pos.x += offset_width;
                
                // Draw hex bytes
                for (std::size_t i = 0; i < BYTES_PER_LINE; i++) {
                    if (i == 8) {
                        // Extra space in the middle
                        pos.x += char_width;
                    }
                    
                    if (i < bytes_in_line) {
                        std::snprintf(line_buf, sizeof(line_buf), "%02X ", raw_content[offset + i]);
                        draw_list->AddText(pos, hex_color, line_buf);
                    } else {
                        // Padding for incomplete lines
                        draw_list->AddText(pos, hex_color, "   ");
                    }
                    pos.x += char_width * 3;
                }
                
                // Separator
                pos.x += char_width;
                draw_list->AddText(pos, hex_color, "|");
                pos.x += char_width * 2;
                
                // Draw ASCII representation (with wider spacing for readability)
                for (std::size_t i = 0; i < bytes_in_line; i++) {
                    unsigned char c = raw_content[offset + i];
                    char ascii_char[2] = {'.', '\0'};
                    ImU32 char_color = non_print_color;
                    
                    if (c >= 32 && c < 127) {
                        ascii_char[0] = static_cast<char>(c);
                        char_color = ascii_color;
                    }
                    
                    draw_list->AddText(pos, char_color, ascii_char);
                    pos.x += ascii_char_spacing;
                }
                
                // Move cursor for next line
                ImGui::Dummy(ImVec2(total_line_width, line_height));
            }
        }
        
        clipper.End();
    }
    
    void TextReader(App &app, bool &properties, bool &file_stat) {
        const float display_w = static_cast<float>(app.gui.display_width);
        const float display_h = static_cast<float>(app.gui.display_height);
        const float bottom_bar_height = 45.0f;
        const float window_h = display_h - bottom_bar_height;
        
        // Set up window to not overlap with bottom bar
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(display_w, window_h), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        
        // Always use NoTitleBar - filename is shown as toast overlay when enabled
        if (ImGui::Begin(app.window.entries[app.window.selected].name, nullptr, 
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar)) {
            
            // Text content area - allow scrolling via touch/mouse and controller
            // Disable nav cursor border (selection border) while keeping focus for scrolling
            ImGui::PushStyleColor(ImGuiCol_NavCursor, IM_COL32(0, 0, 0, 0));
            ImGui::BeginChild("TextContent", ImVec2(0, 0), false, 
                ImGuiWindowFlags_HorizontalScrollbar);
            
            // Auto-focus the content area so scrolling works immediately
            if (ImGui::IsWindowAppearing()) {
                ImGui::SetWindowFocus();
            }
            
            // Sync scroll: if controller changed scroll, apply it; otherwise read ImGui's scroll (for touch/mouse)
            static float last_controller_scroll = 0.0f;
            float current_scroll = TextReader::GetScrollY();
            
            if (current_scroll != last_controller_scroll) {
                // Controller changed the scroll - apply it
                ImGui::SetScrollY(current_scroll);
                last_controller_scroll = current_scroll;
            } else {
                // No controller input - sync from ImGui (touch/mouse scroll)
                float imgui_scroll = ImGui::GetScrollY();
                TextReader::SetScrollY(imgui_scroll);
                last_controller_scroll = imgui_scroll;
            }
            
            // Use monospace-style rendering for better code readability
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            
            // Render content based on mode
            if (TextReader::IsHexMode()) {
                Windows::RenderHexView(app.config);
            } else {
                // Regular text mode
                if (!app.window.text_content.empty()) {
                    ImGui::TextUnformatted(app.window.text_content.c_str(), app.window.text_content.c_str() + app.window.text_content.size());
                }
            }
            
            ImGui::PopStyleVar();
            
            // Update max scroll for input handling
            TextReader::SetMaxScrollY(ImGui::GetScrollMaxY());
            
            ImGui::EndChild();
            ImGui::PopStyleColor();  // Pop NavHighlight
        }
        ImGui::End();
        ImGui::PopStyleVar();
        
        // Draw the bottom bar
        Windows::DrawTextReaderBottomBar(app);
        
        // Draw filename toast overlay (when enabled in settings)
        if (app.config.ImageFilename()) {
            Toast::DrawFilename(app.config, app.window.entries[app.window.selected].name);
        }
        
        if (properties)
            Popups::FilePropertiesPopup(app, file_stat, &properties);
    }
}
