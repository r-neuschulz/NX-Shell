#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "config.hpp"
#include "fs.hpp"
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
    
    bool LoadFile(const std::string &path, bool restore_offset) {
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
            data.text_content = "[File too large to display. Maximum size: 5 MB]";
            s_raw_content.clear();
            return true;  // Still open viewer with message
        }
        
        // Read file content as raw bytes
        s_raw_content.resize(size);
        std::size_t read = fread(s_raw_content.data(), 1, size, file);
        fclose(file);
        
        if (read != size) {
            data.text_content = "[Error reading file]";
            s_raw_content.clear();
            return true;
        }
        
        // Store content as string for text mode
        data.text_content.assign(reinterpret_cast<char*>(s_raw_content.data()), size);
        
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
    
    void Clear(void) {
        data.text_content.clear();
        s_raw_content.clear();
        s_scroll_y = 0.0f;
        s_max_scroll_y = 0.0f;
        s_hex_mode = false;
    }
    
    bool HandleScroll(int index, bool restore_offset) {
        if (data.entries[index].type == FsDirEntryType_Dir)
            return false;
        
        // Check if it's a text or binary file (both can be opened in text reader)
        FileType type = FS::GetFileType(data.entries[index].name);
        if (type != FileTypeText && type != FileTypeBinary)
            return false;
        
        // Auto-enable hex mode for binary files
        s_hex_mode = (type == FileTypeBinary);
        
        data.selected = index;
        std::string path = FS::BuildPath(data.entries[index]);
        return TextReader::LoadFile(path, restore_offset);
    }
    
    bool HandlePrev(void) {
        bool ret = false;
        
        // Save current offset before navigating
        TextReader::SaveScrollOffset();
        
        for (int i = data.selected - 1; i > 0; i--) {
            std::string filename = data.entries[i].name;
            if (filename.empty())
                continue;
            
            if (!(ret = TextReader::HandleScroll(i, true)))  // restore_offset = true
                continue;
            else
                break;
        }
        
        return ret;
    }
    
    bool HandleNext(void) {
        bool ret = false;
        
        if (data.selected == data.entries.size())
            return ret;
        
        // Save current offset before navigating
        TextReader::SaveScrollOffset();
        
        for (unsigned int i = data.selected + 1; i < data.entries.size(); i++) {
            if (!(ret = TextReader::HandleScroll(i, true)))  // restore_offset = true
                continue;
            else
                break;
        }
        
        return ret;
    }
    
    void HandleControls(u64 &key, bool &properties) {
        if (key & HidNpadButton_X)
            properties = true;
        
        if (!properties) {
            if (key & HidNpadButton_B) {
                TextReader::Clear();
                data.state = WINDOW_STATE_FILEBROWSER;
            }
            
            // Y button toggles hex mode
            if (key & HidNpadButton_Y) {
                TextReader::ToggleHexMode();
                s_scroll_y = 0.0f;  // Reset scroll when switching modes
            }
            
            if (key & HidNpadButton_L) {
                TextReader::Clear();
                
                if (!TextReader::HandlePrev())
                    data.state = WINDOW_STATE_FILEBROWSER;
            }
            else if (key & HidNpadButton_R) {
                TextReader::Clear();
                
                if (!TextReader::HandleNext())
                    data.state = WINDOW_STATE_FILEBROWSER;
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
    static void DrawTextReaderBottomBar(void) {
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
        
        // Draw semi-transparent background bar
        const ImU32 bar_bg = is_dark ? IM_COL32(0, 0, 0, 180) : IM_COL32(240, 240, 245, 220);
        draw_list->AddRectFilled(ImVec2(0, bar_y), ImVec2(display_w, display_h), bar_bg);
        
        // Button colors - use style-aware helpers from GUI module
        const ImU32 color_b = GUI::GetButtonColorB();
        const ImU32 color_x = GUI::GetButtonColorX();
        const ImU32 color_text = GUI::GetButtonTextColor();
        const ImU32 color_label = GUI::GetThemeLabelColor();
        const ImU32 color_minus_bg = GUI::GetButtonColorMinus();
        
        // Shoulder button style dimensions
        const float shoulder_width = 32.0f;
        const float shoulder_height = 18.0f;
        const float shoulder_chamfer = 6.0f;
        const ImU32 color_shoulder = is_dark ? IM_COL32(100, 100, 100, 255) : IM_COL32(150, 150, 155, 255);
        
        // Right stick style (for scroll hint)
        const float stick_radius = 10.0f;
        const ImU32 color_dpad = is_dark ? IM_COL32(80, 80, 80, 255) : IM_COL32(140, 140, 145, 255);
        
        // Left side: (-) Quit with hold-to-close indicator
        {
            float left_x = 20.0f;
            ImVec2 minus_center(left_x + button_radius, center_y);
            
            float hold_progress = 0.0f;
            bool is_holding = GUI::IsHoldingToClose(hold_progress);
            
            draw_list->AddCircleFilled(minus_center, button_radius, color_minus_bg, 24);
            
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
            
            const float minus_width = button_radius * 0.8f;
            draw_list->AddLine(
                ImVec2(minus_center.x - minus_width, minus_center.y),
                ImVec2(minus_center.x + minus_width, minus_center.y),
                color_text, 2.0f
            );
            
            float label_x = left_x + button_radius * 2 + 6.0f;
            ImU32 label_color = is_holding 
                ? GUI::GetAccentColorU32WithAlpha(200 + static_cast<int>(55 * hold_progress))
                : color_label;
            draw_list->AddText(ImVec2(label_x, center_y - ImGui::GetTextLineHeight() * 0.5f), label_color, strings[lang][Lang::HintExit]);
        }
        
        // Calculate total width of right-aligned hints
        float total_width = 0.0f;
        total_width += button_radius * 2 + 6.0f + ImGui::CalcTextSize(strings[lang][Lang::HintBack]).x + hint_spacing;
        total_width += button_radius * 2 + 6.0f + ImGui::CalcTextSize(strings[lang][Lang::HintHexMode]).x + hint_spacing;  // Y button for hex
        total_width += button_radius * 2 + 6.0f + ImGui::CalcTextSize(strings[lang][Lang::HintProperties]).x + hint_spacing;
        total_width += shoulder_width + 6.0f + ImGui::CalcTextSize(strings[lang][Lang::HintPrev]).x + hint_spacing;
        total_width += shoulder_width + 6.0f + ImGui::CalcTextSize(strings[lang][Lang::HintNext]).x + hint_spacing;
        // Right stick for scroll
        total_width += stick_radius * 2 + 6.0f + ImGui::CalcTextSize("Scroll").x;
        
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
        
        // Draw Y button (Hex Mode toggle)
        {
            const ImU32 color_y = GUI::GetButtonColorY();
            ImVec2 center(x_offset + button_radius, center_y);
            
            // Draw filled accent ring when hex mode is active
            if (TextReader::IsHexMode()) {
                const float arc_radius = button_radius + 3.0f;
                draw_list->AddCircle(center, arc_radius, GUI::GetAccentColorU32(), 24, 3.0f);
            }
            
            // Draw Y button
            draw_list->AddCircleFilled(center, button_radius, color_y, 24);
            
            // Draw "Y" text centered in the circle
            ImVec2 text_size = ImGui::CalcTextSize("Y");
            draw_list->AddText(ImVec2(center.x - text_size.x * 0.5f + 1.0f, center.y - text_size.y * 0.5f), color_text, "Y");
            
            float label_x = x_offset + button_radius * 2 + 6.0f;
            ImU32 hex_label_color = TextReader::IsHexMode() ? GUI::GetAccentColorU32() : color_label;
            draw_list->AddText(ImVec2(label_x, center_y - ImGui::GetTextLineHeight() * 0.5f), hex_label_color, strings[lang][Lang::HintHexMode]);
            x_offset = label_x + ImGui::CalcTextSize(strings[lang][Lang::HintHexMode]).x + hint_spacing;
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
        
        // Draw Right Stick (Scroll) - circle with vertical arrows
        {
            const float stick_radius = 10.0f;
            ImVec2 stick_center(x_offset + stick_radius, center_y);
            
            // Draw stick circle outline
            draw_list->AddCircle(stick_center, stick_radius, color_dpad, 16, 2.0f);
            
            // Draw small up/down arrows inside the circle
            const float arrow_size = 4.0f;
            // Up arrow
            draw_list->AddTriangleFilled(
                ImVec2(stick_center.x, stick_center.y - arrow_size - 1.0f),
                ImVec2(stick_center.x - arrow_size * 0.6f, stick_center.y - 1.0f),
                ImVec2(stick_center.x + arrow_size * 0.6f, stick_center.y - 1.0f),
                color_dpad
            );
            // Down arrow
            draw_list->AddTriangleFilled(
                ImVec2(stick_center.x, stick_center.y + arrow_size + 1.0f),
                ImVec2(stick_center.x - arrow_size * 0.6f, stick_center.y + 1.0f),
                ImVec2(stick_center.x + arrow_size * 0.6f, stick_center.y + 1.0f),
                color_dpad
            );
            
            float label_x = x_offset + stick_radius * 2 + 6.0f;
            draw_list->AddText(ImVec2(label_x, center_y - ImGui::GetTextLineHeight() * 0.5f), color_label, "Scroll");
        }
    }
    
    // Helper function to render hex view with virtual scrolling for performance
    static void RenderHexView(void) {
        const auto& raw_content = TextReader::GetRawContent();
        if (raw_content.empty()) {
            ImGui::TextUnformatted("[Empty file]");
            return;
        }
        
        const bool is_dark = GUI::IsCurrentThemeDark();
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
    
    void TextReader(bool &properties, bool &file_stat) {
        const float display_w = static_cast<float>(GUI::display_width);
        const float display_h = static_cast<float>(GUI::display_height);
        const float bottom_bar_height = 45.0f;
        const float window_h = display_h - bottom_bar_height;
        
        // Set up window to not overlap with bottom bar
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(display_w, window_h), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        
        // Always use NoTitleBar - filename is shown as toast overlay when enabled
        if (ImGui::Begin(data.entries[data.selected].name, nullptr, 
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
                Windows::RenderHexView();
            } else {
                // Regular text mode
                if (!data.text_content.empty()) {
                    ImGui::TextUnformatted(data.text_content.c_str(), data.text_content.c_str() + data.text_content.size());
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
        Windows::DrawTextReaderBottomBar();
        
        // Draw filename toast overlay (when enabled in settings)
        if (cfg.image_filename) {
            ImDrawList *draw_list = ImGui::GetForegroundDrawList();
            
            const char* filename = data.entries[data.selected].name;
            ImVec2 text_size = ImGui::CalcTextSize(filename);
            
            const float padding_x = 10.0f;
            const float padding_y = 6.0f;
            const float toast_x = 2.0f;
            const float toast_y = 2.0f;
            const float toast_w = text_size.x + padding_x * 2;
            const float toast_h = text_size.y + padding_y * 2;
            
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
        
        if (properties)
            Popups::FilePropertiesPopup(data, file_stat, &properties);
    }
}
