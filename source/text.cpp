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
#include "imgui_stdlib.h"
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
    
    // Edit mode state
    static bool s_edit_mode = false;
    static std::string s_edit_buffer;           // Working copy for text editing
    static bool s_edit_dirty = false;           // Track if content was modified
    
    // Virtual keyboard state (shared between text and hex keyboards)
    static std::vector<int> s_keys_pressed;
    static int s_refocus = 2;                   // Start >= 2 so first frame works
    static bool s_shift_active = false;         // Shift state for QWERTY keyboard
    
    // Hex editor state
    static int s_hex_selected_byte = -1;        // Index into s_raw_content, -1 = none selected
    static bool s_hex_editing_hex = false;       // true = hex column selected, false = ascii column
    static int s_hex_nibble_index = 0;           // 0 = high nibble, 1 = low nibble
    static bool s_hex_kb_visible = false;        // Hex keyboard visibility
    
    // Current file path for saving
    static std::string s_current_path;
    
    bool IsHexMode(void) { return s_hex_mode; }
    void SetHexMode(bool mode) { s_hex_mode = mode; }
    void ToggleHexMode(void) { s_hex_mode = !s_hex_mode; }
    
    bool IsEditMode(void) { return s_edit_mode; }
    void SetEditMode(bool mode) {
        s_edit_mode = mode;
        if (mode) {
            // Enter edit mode: copy content to edit buffer
            s_edit_buffer = std::string(reinterpret_cast<const char*>(s_raw_content.data()), s_raw_content.size());
            s_edit_dirty = false;
            s_hex_selected_byte = -1;
            s_hex_kb_visible = false;
            s_hex_nibble_index = 0;
            s_shift_active = false;
            s_keys_pressed.clear();
            s_refocus = 2;
        } else {
            // Leave edit mode: discard changes
            s_edit_buffer.clear();
            s_edit_dirty = false;
            s_hex_selected_byte = -1;
            s_hex_kb_visible = false;
        }
    }
    void ToggleEditMode(void) { SetEditMode(!s_edit_mode); }
    
    bool SaveFile(App &app) {
        if (s_current_path.empty())
            return false;
        
        FILE *file = fopen(s_current_path.c_str(), "wb");
        if (!file)
            return false;
        
        const unsigned char *data = nullptr;
        std::size_t size = 0;
        
        if (s_hex_mode) {
            // In hex mode, save raw content (which has been edited in-place)
            data = s_raw_content.data();
            size = s_raw_content.size();
        } else {
            // In text mode, save the edit buffer
            data = reinterpret_cast<const unsigned char*>(s_edit_buffer.data());
            size = s_edit_buffer.size();
        }
        
        std::size_t written = fwrite(data, 1, size, file);
        fclose(file);
        
        if (written != size)
            return false;
        
        // Sync the internal buffers after save
        if (!s_hex_mode) {
            s_raw_content.assign(s_edit_buffer.begin(), s_edit_buffer.end());
            app.window.text_content = s_edit_buffer;
        } else {
            app.window.text_content.assign(reinterpret_cast<char*>(s_raw_content.data()), s_raw_content.size());
        }
        
        s_edit_dirty = false;
        return true;
    }
    
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
        
        // Store the file path for saving
        s_current_path = path;
        
        // Reset edit mode state
        s_edit_mode = false;
        s_edit_buffer.clear();
        s_edit_dirty = false;
        s_hex_selected_byte = -1;
        s_hex_kb_visible = false;
        
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
        s_edit_mode = false;
        s_edit_buffer.clear();
        s_edit_dirty = false;
        s_hex_selected_byte = -1;
        s_hex_kb_visible = false;
        s_current_path.clear();
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
                if (s_edit_mode) {
                    // Cancel edit mode (discard changes)
                    SetEditMode(false);
                } else {
                    TextReader::Clear(app);
                    app.window.state = WINDOW_STATE_FILEBROWSER;
                }
            }
            
            // Y button toggles hex mode (only when not in edit mode)
            if (!s_edit_mode && (key & HidNpadButton_Y)) {
                TextReader::ToggleHexMode();
                s_scroll_y = 0.0f;  // Reset scroll when switching modes
            }
            
            // A button: in edit mode = save, otherwise toggle edit mode
            if (key & HidNpadButton_A) {
                if (s_edit_mode) {
                    const int lang = app.config.Lang();
                    if (TextReader::SaveFile(app)) {
                        Toast::Show(strings[lang][Lang::EditorSaveSuccess], true);
                    } else {
                        Toast::Show(strings[lang][Lang::EditorSaveError], false);
                    }
                } else {
                    SetEditMode(true);
                }
            }
            
            if (!s_edit_mode) {
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
            }
            
            // Scroll speed constants (only when not in edit mode, as edit mode uses keyboard)
            if (!s_edit_mode) {
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
    }
    
    float GetScrollY(void) { return s_scroll_y; }
    void SetScrollY(float y) { s_scroll_y = y; }
    void SetMaxScrollY(float max_y) { s_max_scroll_y = max_y; }
    
    const std::vector<unsigned char>& GetRawContent(void) { return s_raw_content; }
}

namespace Windows {
    // ========================================================================
    // Virtual QWERTY Keyboard
    // mode: 0 = text edit (uses refocus trick with InputText)
    //       1 = hex ASCII (directly modifies byte in raw_content)
    // ========================================================================
    static void RenderVirtualKeyboard(ConfigService &config_svc, float display_w, int mode) {
        const bool is_dark = GUI::IsCurrentThemeDark(config_svc);
        
        // Theme-aware button colors
        const ImU32 btn_color = is_dark ? IM_COL32(60, 60, 70, 255) : IM_COL32(220, 220, 230, 255);
        const ImU32 btn_hover = is_dark ? IM_COL32(80, 80, 95, 255) : IM_COL32(200, 200, 215, 255);
        const ImU32 btn_active = is_dark ? IM_COL32(100, 100, 120, 255) : IM_COL32(180, 180, 200, 255);
        const ImU32 btn_shift = TextReader::s_shift_active ? 
            GUI::GetAccentColorU32(config_svc) : btn_color;
        
        ImGui::PushStyleColor(ImGuiCol_Button, btn_color);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, btn_hover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, btn_active);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(3.0f, 3.0f));
        
        // Calculate key size based on available width
        const float avail_w = display_w - 20.0f;  // 10px padding each side
        const float key_w = (avail_w - 12 * 3.0f) / 13.0f;  // 13 keys per row max (numbers + backspace)
        const float key_h = 32.0f;
        const ImVec2 key_size(key_w, key_h);
        
        // Lambda: handle a key press depending on mode
        auto OnKeyPress = [&](int key_code) {
            if (mode == 0) {
                // Text edit mode: buffer keys for refocus trick
                TextReader::s_refocus = 0;
                TextReader::s_keys_pressed.push_back(key_code);
            } else {
                // Hex ASCII mode: directly modify the selected byte
                if (TextReader::s_hex_selected_byte >= 0 &&
                    TextReader::s_hex_selected_byte < static_cast<int>(TextReader::s_raw_content.size())) {
                    if (key_code == ImGuiKey_Backspace) {
                        // Go back one byte
                        if (TextReader::s_hex_selected_byte > 0) {
                            TextReader::s_hex_selected_byte--;
                        }
                    } else if (key_code == ImGuiKey_Enter) {
                        // Advance to next byte
                        if (TextReader::s_hex_selected_byte < static_cast<int>(TextReader::s_raw_content.size()) - 1) {
                            TextReader::s_hex_selected_byte++;
                        }
                    } else if (key_code >= 32 && key_code < 127) {
                        TextReader::s_raw_content[TextReader::s_hex_selected_byte] = static_cast<unsigned char>(key_code);
                        TextReader::s_edit_dirty = true;
                        // Auto-advance
                        if (TextReader::s_hex_selected_byte < static_cast<int>(TextReader::s_raw_content.size()) - 1) {
                            TextReader::s_hex_selected_byte++;
                        }
                    }
                }
            }
        };
        
        // Lambda to create a key button
        auto PressKey = [&](const char *label, int key_code, const ImVec2 &size) -> bool {
            if (ImGui::Button(label, size)) {
                OnKeyPress(key_code);
                return true;
            }
            return false;
        };
        
        // Lambda for a row of character keys
        auto KeyboardLine = [&](const char *keys, float indent = 0.0f) {
            if (indent > 0.0f) {
                ImGui::Dummy(ImVec2(indent, 0.0f));
                ImGui::SameLine();
            }
            std::size_t num = strlen(keys);
            for (std::size_t i = 0; i < num; i++) {
                char key_label[2] = { keys[i], '\0' };
                int key_code = keys[i];
                if (TextReader::s_shift_active && keys[i] >= 'a' && keys[i] <= 'z') {
                    key_label[0] = keys[i] - 32;  // uppercase
                    key_code = keys[i] - 32;
                }
                PressKey(key_label, key_code, key_size);
                if (i < num - 1) ImGui::SameLine();
            }
        };
        
        // Row 1: Numbers + Backspace
        KeyboardLine("1234567890-");
        ImGui::SameLine();
        ImVec2 bksp_size(key_w * 2.0f + 3.0f, key_h);
        PressKey("<--", ImGuiKey_Backspace, bksp_size);
        
        // Row 2: qwertyuiop
        float row2_indent = key_w * 0.25f;
        KeyboardLine("qwertyuiop", row2_indent);
        
        // Row 3: asdfghjkl + Enter
        float row3_indent = key_w * 0.5f;
        KeyboardLine("asdfghjkl", row3_indent);
        ImGui::SameLine();
        ImVec2 enter_size(key_w * 2.0f + 3.0f, key_h);
        PressKey("Enter", ImGuiKey_Enter, enter_size);
        
        // Row 4: Shift + zxcvbnm,./ 
        {
            // Shift button with special color when active
            if (TextReader::s_shift_active) {
                ImGui::PushStyleColor(ImGuiCol_Button, btn_shift);
            }
            ImVec2 shift_size(key_w * 1.5f + 3.0f, key_h);
            if (ImGui::Button("Shift", shift_size)) {
                TextReader::s_shift_active = !TextReader::s_shift_active;
            }
            if (TextReader::s_shift_active) {
                ImGui::PopStyleColor();
            }
            ImGui::SameLine();
        }
        KeyboardLine("zxcvbnm,./");
        
        // Row 5: Space bar
        {
            float space_indent = key_w * 2.0f;
            ImGui::Dummy(ImVec2(space_indent, 0.0f));
            ImGui::SameLine();
            ImVec2 space_size(avail_w - space_indent * 2.0f, key_h);
            PressKey("Space", ' ', space_size);
        }
        
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
    }
    
    // ========================================================================
    // Hex Virtual Keyboard (2-row compact)
    // ========================================================================
    static void RenderHexKeyboard(ConfigService &config_svc) {
        const bool is_dark = GUI::IsCurrentThemeDark(config_svc);
        
        const ImU32 btn_color = is_dark ? IM_COL32(60, 60, 70, 255) : IM_COL32(220, 220, 230, 255);
        const ImU32 btn_hover = is_dark ? IM_COL32(80, 80, 95, 255) : IM_COL32(200, 200, 215, 255);
        const ImU32 btn_active = is_dark ? IM_COL32(100, 100, 120, 255) : IM_COL32(180, 180, 200, 255);
        
        ImGui::PushStyleColor(ImGuiCol_Button, btn_color);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, btn_hover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, btn_active);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f, 2.0f));
        
        const float key_w = 28.0f;
        const float key_h = 28.0f;
        const ImVec2 key_size(key_w, key_h);
        
        // Row 1: 0-9
        const char *row1 = "0123456789";
        for (int i = 0; i < 10; i++) {
            char label[2] = { row1[i], '\0' };
            if (ImGui::Button(label, key_size)) {
                int nibble_val = row1[i] - '0';
                if (TextReader::s_hex_selected_byte >= 0 && 
                    TextReader::s_hex_selected_byte < static_cast<int>(TextReader::s_raw_content.size())) {
                    unsigned char &byte = TextReader::s_raw_content[TextReader::s_hex_selected_byte];
                    if (TextReader::s_hex_nibble_index == 0) {
                        byte = (byte & 0x0F) | (static_cast<unsigned char>(nibble_val) << 4);
                        TextReader::s_hex_nibble_index = 1;
                    } else {
                        byte = (byte & 0xF0) | static_cast<unsigned char>(nibble_val);
                        TextReader::s_hex_nibble_index = 0;
                        // Auto-advance to next byte
                        if (TextReader::s_hex_selected_byte < static_cast<int>(TextReader::s_raw_content.size()) - 1) {
                            TextReader::s_hex_selected_byte++;
                        }
                    }
                    TextReader::s_edit_dirty = true;
                }
            }
            if (i < 9) ImGui::SameLine();
        }
        
        // Row 2: A-F + < (backspace) + > (advance)
        const char *row2 = "ABCDEF";
        for (int i = 0; i < 6; i++) {
            char label[2] = { row2[i], '\0' };
            if (ImGui::Button(label, key_size)) {
                int nibble_val = 10 + (row2[i] - 'A');
                if (TextReader::s_hex_selected_byte >= 0 && 
                    TextReader::s_hex_selected_byte < static_cast<int>(TextReader::s_raw_content.size())) {
                    unsigned char &byte = TextReader::s_raw_content[TextReader::s_hex_selected_byte];
                    if (TextReader::s_hex_nibble_index == 0) {
                        byte = (byte & 0x0F) | (static_cast<unsigned char>(nibble_val) << 4);
                        TextReader::s_hex_nibble_index = 1;
                    } else {
                        byte = (byte & 0xF0) | static_cast<unsigned char>(nibble_val);
                        TextReader::s_hex_nibble_index = 0;
                        // Auto-advance to next byte
                        if (TextReader::s_hex_selected_byte < static_cast<int>(TextReader::s_raw_content.size()) - 1) {
                            TextReader::s_hex_selected_byte++;
                        }
                    }
                    TextReader::s_edit_dirty = true;
                }
            }
            ImGui::SameLine();
        }
        
        // < button (go back one byte)
        if (ImGui::Button("<", key_size)) {
            if (TextReader::s_hex_selected_byte > 0) {
                TextReader::s_hex_selected_byte--;
                TextReader::s_hex_nibble_index = 0;
            }
        }
        ImGui::SameLine();
        
        // > button (advance one byte)
        if (ImGui::Button(">", key_size)) {
            if (TextReader::s_hex_selected_byte >= 0 && 
                TextReader::s_hex_selected_byte < static_cast<int>(TextReader::s_raw_content.size()) - 1) {
                TextReader::s_hex_selected_byte++;
                TextReader::s_hex_nibble_index = 0;
            }
        }
        
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
    }
    
    // ========================================================================
    // Bottom Bar (adapted for edit/view mode)
    // ========================================================================
    static void DrawTextReaderBottomBar(App &app) {
        const int lang = app.config.Lang();
        
        if (TextReader::IsEditMode()) {
            // Edit mode bottom bar
            std::vector<BottomBar::HintItem> left_items = {
                {BottomBar::ButtonType::Minus, strings[lang][Lang::HintExit]}
            };
            
            std::vector<BottomBar::HintItem> right_items = {
                {BottomBar::ButtonType::CircleB, strings[lang][Lang::HintBack]},
                {BottomBar::ButtonType::CircleA, strings[lang][Lang::HintSave]},
            };
            
            BottomBar::Config config;
            config.use_foreground_draw_list = true;
            config.draw_background = true;
            
            BottomBar::Draw(app.config, config, left_items, right_items);
        } else {
            // View mode bottom bar (original)
            std::vector<BottomBar::HintItem> left_items = {
                {BottomBar::ButtonType::Minus, strings[lang][Lang::HintExit]}
            };
            
            std::vector<BottomBar::HintItem> right_items = {
                {BottomBar::ButtonType::CircleB, strings[lang][Lang::HintBack]},
                {BottomBar::ButtonType::CircleA, strings[lang][Lang::HintEdit]},
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
    }
    
    // ========================================================================
    // Hex View with click detection for editing
    // ========================================================================
    static void RenderHexView(App &app) {
        auto &raw_content = TextReader::s_raw_content;
        if (raw_content.empty()) {
            ImGui::TextUnformatted("[Empty file]");
            return;
        }
        
        ConfigService &config_svc = app.config;
        const bool is_dark = GUI::IsCurrentThemeDark(config_svc);
        const ImU32 offset_color = is_dark ? IM_COL32(100, 180, 255, 255) : IM_COL32(0, 100, 200, 255);
        const ImU32 hex_color = is_dark ? IM_COL32(220, 220, 220, 255) : IM_COL32(30, 30, 30, 255);
        const ImU32 ascii_color = is_dark ? IM_COL32(180, 255, 180, 255) : IM_COL32(0, 130, 0, 255);
        const ImU32 non_print_color = is_dark ? IM_COL32(100, 100, 100, 255) : IM_COL32(180, 180, 180, 255);
        const ImU32 sel_bg_color = is_dark ? IM_COL32(80, 80, 150, 180) : IM_COL32(150, 150, 230, 180);
        
        constexpr int BYTES_PER_LINE = 16;
        const int total_lines = static_cast<int>((raw_content.size() + BYTES_PER_LINE - 1) / BYTES_PER_LINE);
        
        const float char_width = ImGui::CalcTextSize("0").x;
        const float ascii_char_spacing = char_width * 1.4f;
        const float offset_width = char_width * 10;
        const float hex_section_width = char_width * (BYTES_PER_LINE * 3 + 2);
        const float line_height = ImGui::GetTextLineHeight();
        const float total_line_width = offset_width + hex_section_width + ascii_char_spacing * BYTES_PER_LINE + char_width * 4;
        
        const bool editing = TextReader::IsEditMode();
        
        char line_buf[256];
        
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
                
                // Draw hex bytes with click detection
                for (std::size_t i = 0; i < BYTES_PER_LINE; i++) {
                    if (i == 8) {
                        pos.x += char_width;
                    }
                    
                    if (i < bytes_in_line) {
                        int byte_index = static_cast<int>(offset + i);
                        bool is_selected = editing && (byte_index == TextReader::s_hex_selected_byte) && TextReader::s_hex_editing_hex;
                        
                        // Draw selection highlight
                        if (is_selected) {
                            ImVec2 sel_min(pos.x - 1, pos.y);
                            ImVec2 sel_max(pos.x + char_width * 2 + 1, pos.y + line_height);
                            draw_list->AddRectFilled(sel_min, sel_max, sel_bg_color, 2.0f);
                        }
                        
                        std::snprintf(line_buf, sizeof(line_buf), "%02X ", raw_content[byte_index]);
                        draw_list->AddText(pos, hex_color, line_buf);
                        
                        // Click detection for hex column
                        if (editing) {
                            ImVec2 click_min(pos.x, pos.y);
                            ImVec2 click_max(pos.x + char_width * 3, pos.y + line_height);
                            
                            if (ImGui::IsMouseClicked(0)) {
                                ImVec2 mouse = ImGui::GetMousePos();
                                if (mouse.x >= click_min.x && mouse.x < click_max.x &&
                                    mouse.y >= click_min.y && mouse.y < click_max.y) {
                                    TextReader::s_hex_selected_byte = byte_index;
                                    TextReader::s_hex_editing_hex = true;
                                    TextReader::s_hex_nibble_index = 0;
                                    TextReader::s_hex_kb_visible = true;
                                }
                            }
                        }
                    } else {
                        draw_list->AddText(pos, hex_color, "   ");
                    }
                    pos.x += char_width * 3;
                }
                
                // Separator
                pos.x += char_width;
                draw_list->AddText(pos, hex_color, "|");
                pos.x += char_width * 2;
                
                // Draw ASCII representation with click detection
                for (std::size_t i = 0; i < bytes_in_line; i++) {
                    int byte_index = static_cast<int>(offset + i);
                    unsigned char c = raw_content[byte_index];
                    char ascii_char[2] = {'.', '\0'};
                    ImU32 char_color = non_print_color;
                    
                    if (c >= 32 && c < 127) {
                        ascii_char[0] = static_cast<char>(c);
                        char_color = ascii_color;
                    }
                    
                    bool is_selected = editing && (byte_index == TextReader::s_hex_selected_byte) && !TextReader::s_hex_editing_hex;
                    
                    if (is_selected) {
                        ImVec2 sel_min(pos.x - 1, pos.y);
                        ImVec2 sel_max(pos.x + ascii_char_spacing, pos.y + line_height);
                        draw_list->AddRectFilled(sel_min, sel_max, sel_bg_color, 2.0f);
                    }
                    
                    draw_list->AddText(pos, char_color, ascii_char);
                    
                    // Click detection for ASCII column
                    if (editing) {
                        ImVec2 click_min(pos.x, pos.y);
                        ImVec2 click_max(pos.x + ascii_char_spacing, pos.y + line_height);
                        
                        if (ImGui::IsMouseClicked(0)) {
                            ImVec2 mouse = ImGui::GetMousePos();
                            if (mouse.x >= click_min.x && mouse.x < click_max.x &&
                                mouse.y >= click_min.y && mouse.y < click_max.y) {
                                TextReader::s_hex_selected_byte = byte_index;
                                TextReader::s_hex_editing_hex = false;
                                TextReader::s_hex_nibble_index = 0;
                                TextReader::s_hex_kb_visible = true;
                                // For ASCII: the main QWERTY keyboard will be used via refocus trick
                                TextReader::s_refocus = 0;
                            }
                        }
                    }
                    
                    pos.x += ascii_char_spacing;
                }
                
                ImGui::Dummy(ImVec2(total_line_width, line_height));
            }
        }
        
        clipper.End();
    }
    
    // ========================================================================
    // Main TextReader Window
    // ========================================================================
    void TextReader(App &app, bool &properties, bool &file_stat) {
        const float display_w = static_cast<float>(app.gui.display_width);
        const float display_h = static_cast<float>(app.gui.display_height);
        const float bottom_bar_height = 45.0f;
        
        const bool edit_mode = TextReader::IsEditMode();
        const bool hex_mode = TextReader::IsHexMode();
        
        // Calculate keyboard heights
        // In hex mode: show hex keyboard for hex column, QWERTY keyboard for ASCII column
        const bool hex_ascii_selected = edit_mode && hex_mode && TextReader::s_hex_kb_visible && !TextReader::s_hex_editing_hex;
        const bool hex_hex_selected = edit_mode && hex_mode && TextReader::s_hex_kb_visible && TextReader::s_hex_editing_hex;
        const float text_kb_height = (edit_mode && !hex_mode) || hex_ascii_selected ? 185.0f : 0.0f;
        const float hex_kb_height = hex_hex_selected ? 70.0f : 0.0f;
        const float keyboard_height = text_kb_height + hex_kb_height;
        
        const float window_h = display_h - bottom_bar_height - keyboard_height;
        
        // Set up window
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(display_w, window_h), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        
        if (ImGui::Begin(app.window.entries[app.window.selected].name, nullptr, 
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar)) {
            
            ImGui::PushStyleColor(ImGuiCol_NavCursor, IM_COL32(0, 0, 0, 0));
            
            if (edit_mode && !hex_mode) {
                // ============================================================
                // TEXT EDIT MODE: InputTextMultiline + virtual keyboard
                // ============================================================
                ImGuiIO &io = ImGui::GetIO();
                
                // Refocus mechanism for virtual keyboard
                if (TextReader::s_refocus == 0) {
                    ImGui::SetKeyboardFocusHere();
                } else if (TextReader::s_refocus >= 2) {
                    // Inject buffered keys from virtual keyboard
                    while (!TextReader::s_keys_pressed.empty()) {
                        int k = TextReader::s_keys_pressed[0];
                        if (k == ImGuiKey_Backspace) {
                            io.AddKeyEvent(ImGuiKey_Backspace, true);
                            io.AddKeyEvent(ImGuiKey_Backspace, false);
                        } else if (k == ImGuiKey_Enter) {
                            io.AddInputCharacter('\n');
                        } else {
                            io.AddInputCharacter(static_cast<unsigned int>(k));
                        }
                        TextReader::s_keys_pressed.erase(TextReader::s_keys_pressed.begin());
                    }
                }
                TextReader::s_refocus++;
                
                // Allocate a resizable buffer for InputTextMultiline
                // ImGui's InputTextMultiline needs a char buffer, we use the callback approach
                float content_h = window_h - ImGui::GetCursorPosY() - 10.0f;
                if (content_h < 100.0f) content_h = 100.0f;
                
                ImGui::InputTextMultiline("##textedit", &TextReader::s_edit_buffer,
                    ImVec2(display_w - 16.0f, content_h),
                    ImGuiInputTextFlags_AllowTabInput);
                
                // Track if content was modified
                if (ImGui::IsItemEdited()) {
                    TextReader::s_edit_dirty = true;
                }
                
            } else if (edit_mode && hex_mode) {
                // ============================================================
                // HEX EDIT MODE: Hex view with click detection
                // ============================================================
                
                // Handle USB keyboard input for ASCII column editing
                // (Virtual keyboard uses mode 1 which directly modifies bytes)
                if (TextReader::s_hex_selected_byte >= 0 && !TextReader::s_hex_editing_hex) {
                    ImGuiIO &io = ImGui::GetIO();
                    for (int n = 0; n < io.InputQueueCharacters.Size; n++) {
                        unsigned int c = io.InputQueueCharacters[n];
                        if (c >= 32 && c < 127) {
                            TextReader::s_raw_content[TextReader::s_hex_selected_byte] = static_cast<unsigned char>(c);
                            TextReader::s_edit_dirty = true;
                            if (TextReader::s_hex_selected_byte < static_cast<int>(TextReader::s_raw_content.size()) - 1) {
                                TextReader::s_hex_selected_byte++;
                            }
                        }
                    }
                    io.InputQueueCharacters.resize(0);
                }
                
                // Handle USB keyboard input for hex column editing (0-9, A-F keys)
                if (TextReader::s_hex_selected_byte >= 0 && TextReader::s_hex_editing_hex) {
                    ImGuiIO &io = ImGui::GetIO();
                    for (int n = 0; n < io.InputQueueCharacters.Size; n++) {
                        unsigned int c = io.InputQueueCharacters[n];
                        int nibble_val = -1;
                        if (c >= '0' && c <= '9') nibble_val = c - '0';
                        else if (c >= 'a' && c <= 'f') nibble_val = 10 + (c - 'a');
                        else if (c >= 'A' && c <= 'F') nibble_val = 10 + (c - 'A');
                        
                        if (nibble_val >= 0 && TextReader::s_hex_selected_byte < static_cast<int>(TextReader::s_raw_content.size())) {
                            unsigned char &byte = TextReader::s_raw_content[TextReader::s_hex_selected_byte];
                            if (TextReader::s_hex_nibble_index == 0) {
                                byte = (byte & 0x0F) | (static_cast<unsigned char>(nibble_val) << 4);
                                TextReader::s_hex_nibble_index = 1;
                            } else {
                                byte = (byte & 0xF0) | static_cast<unsigned char>(nibble_val);
                                TextReader::s_hex_nibble_index = 0;
                                if (TextReader::s_hex_selected_byte < static_cast<int>(TextReader::s_raw_content.size()) - 1) {
                                    TextReader::s_hex_selected_byte++;
                                }
                            }
                            TextReader::s_edit_dirty = true;
                        }
                    }
                    io.InputQueueCharacters.resize(0);
                }
                
                ImGui::BeginChild("HexContent", ImVec2(0, 0), false, 
                    ImGuiWindowFlags_HorizontalScrollbar);
                
                // Sync scroll
                if (ImGui::IsWindowAppearing()) {
                    ImGui::SetWindowFocus();
                }
                
                static float last_controller_scroll_hex = 0.0f;
                float current_scroll = TextReader::GetScrollY();
                
                if (current_scroll != last_controller_scroll_hex) {
                    ImGui::SetScrollY(current_scroll);
                    last_controller_scroll_hex = current_scroll;
                } else {
                    float imgui_scroll = ImGui::GetScrollY();
                    TextReader::SetScrollY(imgui_scroll);
                    last_controller_scroll_hex = imgui_scroll;
                }
                
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
                Windows::RenderHexView(app);
                ImGui::PopStyleVar();
                
                TextReader::SetMaxScrollY(ImGui::GetScrollMaxY());
                
                ImGui::EndChild();
            } else {
                // ============================================================
                // VIEW MODE (no editing): Original read-only rendering
                // ============================================================
                ImGui::BeginChild("TextContent", ImVec2(0, 0), false, 
                    ImGuiWindowFlags_HorizontalScrollbar);
                
                if (ImGui::IsWindowAppearing()) {
                    ImGui::SetWindowFocus();
                }
                
                // Sync scroll
                static float last_controller_scroll = 0.0f;
                float current_scroll = TextReader::GetScrollY();
                
                if (current_scroll != last_controller_scroll) {
                    ImGui::SetScrollY(current_scroll);
                    last_controller_scroll = current_scroll;
                } else {
                    float imgui_scroll = ImGui::GetScrollY();
                    TextReader::SetScrollY(imgui_scroll);
                    last_controller_scroll = imgui_scroll;
                }
                
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
                
                if (hex_mode) {
                    Windows::RenderHexView(app);
                } else {
                    if (!app.window.text_content.empty()) {
                        ImGui::TextUnformatted(app.window.text_content.c_str(), 
                            app.window.text_content.c_str() + app.window.text_content.size());
                    }
                }
                
                ImGui::PopStyleVar();
                TextReader::SetMaxScrollY(ImGui::GetScrollMaxY());
                
                ImGui::EndChild();
            }
            
            ImGui::PopStyleColor();  // Pop NavCursor
        }
        ImGui::End();
        ImGui::PopStyleVar();
        
        // ================================================================
        // Draw Virtual Keyboard Panel (below content, above bottom bar)
        // ================================================================
        if (text_kb_height > 0.0f) {
            // QWERTY keyboard: text edit mode OR hex ASCII column editing
            int kb_mode = (edit_mode && !hex_mode) ? 0 : 1;  // 0 = text InputText, 1 = hex ASCII direct
            
            ImGui::SetNextWindowPos(ImVec2(0.0f, window_h), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(display_w, text_kb_height), ImGuiCond_Always);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 5.0f));
            
            const bool is_dark = GUI::IsCurrentThemeDark(app.config);
            ImU32 kb_bg = is_dark ? IM_COL32(35, 35, 45, 245) : IM_COL32(240, 240, 245, 245);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, kb_bg);
            
            if (ImGui::Begin("##VirtualKeyboard", nullptr,
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | 
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar)) {
                Windows::RenderVirtualKeyboard(app.config, display_w, kb_mode);
            }
            ImGui::End();
            ImGui::PopStyleColor();
            ImGui::PopStyleVar(2);
        }
        
        if (hex_kb_height > 0.0f) {
            // Hex keyboard (compact, at bottom-left)
            float hex_kb_width = 310.0f;  // Compact width for hex keyboard
            ImGui::SetNextWindowPos(ImVec2(0.0f, window_h), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(hex_kb_width, hex_kb_height), ImGuiCond_Always);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5.0f, 5.0f));
            
            const bool is_dark_hex = GUI::IsCurrentThemeDark(app.config);
            ImU32 kb_bg_hex = is_dark_hex ? IM_COL32(35, 35, 45, 245) : IM_COL32(240, 240, 245, 245);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, kb_bg_hex);
            
            if (ImGui::Begin("##HexKeyboard", nullptr,
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | 
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar)) {
                
                // Show which byte is selected
                if (TextReader::s_hex_selected_byte >= 0) {
                    char info[64];
                    std::snprintf(info, sizeof(info), "Byte %04X [%s nibble]", 
                        TextReader::s_hex_selected_byte,
                        TextReader::s_hex_nibble_index == 0 ? "hi" : "lo");
                    ImGui::TextUnformatted(info);
                }
                
                Windows::RenderHexKeyboard(app.config);
            }
            ImGui::End();
            ImGui::PopStyleColor();
            ImGui::PopStyleVar(2);
        }
        
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
