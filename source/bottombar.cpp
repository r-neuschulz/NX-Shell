#include <cmath>

#include "bottombar.hpp"
#include "config.hpp"
#include "gui.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "language.hpp"

namespace BottomBar {
    // Internal constants
    static constexpr float SHOULDER_WIDTH = 32.0f;
    static constexpr float SHOULDER_HEIGHT = 18.0f;
    static constexpr float SHOULDER_CHAMFER = 6.0f;
    static constexpr float ZR_WIDTH = 32.0f;
    static constexpr float ZR_HEIGHT = 18.0f;
    static constexpr float ZR_CHAMFER = 6.0f;
    static constexpr float DPAD_SIZE = 10.0f;
    static constexpr float STICK_RADIUS = 10.0f;

    // Draw a progress arc around a center point (used for hold-to-close)
    static void DrawProgressArc(ImDrawList* draw_list, ImVec2 center, float radius, float progress, ImU32 color) {
        const float start_angle = -IM_PI * 0.5f;  // Start from top
        const float end_angle = start_angle + (progress * IM_PI * 2.0f);
        const int num_segments = static_cast<int>(24 * progress) + 1;
        
        for (int i = 0; i < num_segments; i++) {
            float a1 = start_angle + (end_angle - start_angle) * (static_cast<float>(i) / num_segments);
            float a2 = start_angle + (end_angle - start_angle) * (static_cast<float>(i + 1) / num_segments);
            ImVec2 p1(center.x + cosf(a1) * radius, center.y + sinf(a1) * radius);
            ImVec2 p2(center.x + cosf(a2) * radius, center.y + sinf(a2) * radius);
            draw_list->AddLine(p1, p2, color, 3.0f);
        }
    }

    // Draw a circle button (A, B, X, Y, +, -)
    static float DrawCircleButton(ImDrawList* draw_list, float x, float center_y, float radius,
                                  const char* letter, ImU32 bg_color, ImU32 text_color, 
                                  const char* label, ImU32 label_color, bool active) {
        ImVec2 center(x + radius, center_y);
        
        // Draw active ring if enabled
        if (active) {
            draw_list->AddCircle(center, radius + 3.0f, GUI::GetAccentColorU32(), 24, 3.0f);
        }
        
        // Draw filled circle background
        draw_list->AddCircleFilled(center, radius, bg_color, 24);
        
        // Draw letter centered
        ImVec2 text_size = ImGui::CalcTextSize(letter);
        draw_list->AddText(ImVec2(center.x - text_size.x * 0.5f + 1.0f, center.y - text_size.y * 0.5f), text_color, letter);
        
        // Draw label
        float label_x = x + radius * 2 + 6.0f;
        ImU32 final_label_color = active ? GUI::GetAccentColorU32() : label_color;
        draw_list->AddText(ImVec2(label_x, center_y - ImGui::GetTextLineHeight() * 0.5f), final_label_color, label);
        
        // Return total width consumed
        return radius * 2 + 6.0f + ImGui::CalcTextSize(label).x;
    }

    // Draw the minus button with hold-to-close progress arc
    static float DrawMinusButton(ImDrawList* draw_list, float x, float center_y, float radius,
                                 const char* label, ImU32 label_color) {
        const ImU32 color_minus_bg = GUI::GetButtonColorMinus();
        const ImU32 color_text = GUI::GetButtonTextColor();
        
        ImVec2 center(x + radius, center_y);
        
        // Check hold-to-close state
        float hold_progress = 0.0f;
        bool is_holding = GUI::IsHoldingToClose(hold_progress);
        
        // Draw background circle
        draw_list->AddCircleFilled(center, radius, color_minus_bg, 24);
        
        // Draw progress arc when holding
        if (is_holding && hold_progress > 0.0f) {
            DrawProgressArc(draw_list, center, radius + 3.0f, hold_progress, GUI::GetAccentColorU32());
        }
        
        // Draw minus sign
        const float minus_width = radius * 0.8f;
        draw_list->AddLine(
            ImVec2(center.x - minus_width, center.y),
            ImVec2(center.x + minus_width, center.y),
            color_text, 2.0f
        );
        
        // Draw label with hold-aware coloring
        float label_x = x + radius * 2 + 6.0f;
        ImU32 final_label_color = is_holding 
            ? GUI::GetAccentColorU32WithAlpha(200 + static_cast<int>(55 * hold_progress))
            : label_color;
        draw_list->AddText(ImVec2(label_x, center_y - ImGui::GetTextLineHeight() * 0.5f), final_label_color, label);
        
        return radius * 2 + 6.0f + ImGui::CalcTextSize(label).x;
    }

    // Draw the plus button (can have refresh animation)
    static float DrawPlusButton(ImDrawList* draw_list, float x, float center_y, float radius,
                                const char* label, ImU32 label_color, bool show_refresh_anim) {
        const ImU32 color_plus_bg = GUI::GetButtonColorPlus();
        const ImU32 color_text = GUI::GetButtonTextColor();
        
        ImVec2 center(x + radius, center_y);
        
        // Draw background circle
        draw_list->AddCircleFilled(center, radius, color_plus_bg, 24);
        
        // Draw refresh animation arc if active
        if (show_refresh_anim) {
            float refresh_progress = 0.0f;
            if (GUI::IsRefreshAnimating(refresh_progress)) {
                DrawProgressArc(draw_list, center, radius + 3.0f, refresh_progress, GUI::GetAccentColorU32());
            }
        }
        
        // Draw plus sign
        const float plus_width = radius * 0.8f;
        draw_list->AddLine(
            ImVec2(center.x - plus_width, center.y),
            ImVec2(center.x + plus_width, center.y),
            color_text, 2.0f
        );
        draw_list->AddLine(
            ImVec2(center.x, center.y - plus_width),
            ImVec2(center.x, center.y + plus_width),
            color_text, 2.0f
        );
        
        // Draw label
        float label_x = x + radius * 2 + 6.0f;
        draw_list->AddText(ImVec2(label_x, center_y - ImGui::GetTextLineHeight() * 0.5f), label_color, label);
        
        return radius * 2 + 6.0f + ImGui::CalcTextSize(label).x;
    }

    // Draw L shoulder button (chamfered on left)
    static float DrawShoulderL(ImDrawList* draw_list, float x, float center_y,
                               const char* label, ImU32 label_color) {
        const bool is_dark = GUI::IsCurrentThemeDark();
        const ImU32 color_shoulder = is_dark ? IM_COL32(100, 100, 100, 255) : IM_COL32(150, 150, 155, 255);
        const ImU32 color_text = GUI::GetButtonTextColor();
        
        float btn_y = center_y - SHOULDER_HEIGHT * 0.5f;
        
        // L button: chamfer on top-left corner
        ImVec2 pts[5] = {
            ImVec2(x + SHOULDER_CHAMFER, btn_y),
            ImVec2(x + SHOULDER_WIDTH, btn_y),
            ImVec2(x + SHOULDER_WIDTH, btn_y + SHOULDER_HEIGHT),
            ImVec2(x, btn_y + SHOULDER_HEIGHT),
            ImVec2(x, btn_y + SHOULDER_CHAMFER)
        };
        draw_list->AddConvexPolyFilled(pts, 5, color_shoulder);
        
        // Draw "L" text centered
        ImVec2 text_size = ImGui::CalcTextSize("L");
        draw_list->AddText(ImVec2(x + SHOULDER_WIDTH * 0.5f - text_size.x * 0.5f, center_y - text_size.y * 0.5f), color_text, "L");
        
        // Draw label
        float label_x = x + SHOULDER_WIDTH + 6.0f;
        draw_list->AddText(ImVec2(label_x, center_y - ImGui::GetTextLineHeight() * 0.5f), label_color, label);
        
        return SHOULDER_WIDTH + 6.0f + ImGui::CalcTextSize(label).x;
    }

    // Draw R shoulder button (chamfered on right)
    static float DrawShoulderR(ImDrawList* draw_list, float x, float center_y,
                               const char* label, ImU32 label_color) {
        const bool is_dark = GUI::IsCurrentThemeDark();
        const ImU32 color_shoulder = is_dark ? IM_COL32(100, 100, 100, 255) : IM_COL32(150, 150, 155, 255);
        const ImU32 color_text = GUI::GetButtonTextColor();
        
        float btn_y = center_y - SHOULDER_HEIGHT * 0.5f;
        
        // R button: chamfer on top-right corner
        ImVec2 pts[5] = {
            ImVec2(x, btn_y),
            ImVec2(x + SHOULDER_WIDTH - SHOULDER_CHAMFER, btn_y),
            ImVec2(x + SHOULDER_WIDTH, btn_y + SHOULDER_CHAMFER),
            ImVec2(x + SHOULDER_WIDTH, btn_y + SHOULDER_HEIGHT),
            ImVec2(x, btn_y + SHOULDER_HEIGHT)
        };
        draw_list->AddConvexPolyFilled(pts, 5, color_shoulder);
        
        // Draw "R" text centered
        ImVec2 text_size = ImGui::CalcTextSize("R");
        draw_list->AddText(ImVec2(x + SHOULDER_WIDTH * 0.5f - text_size.x * 0.5f, center_y - text_size.y * 0.5f), color_text, "R");
        
        // Draw label
        float label_x = x + SHOULDER_WIDTH + 6.0f;
        draw_list->AddText(ImVec2(label_x, center_y - ImGui::GetTextLineHeight() * 0.5f), label_color, label);
        
        return SHOULDER_WIDTH + 6.0f + ImGui::CalcTextSize(label).x;
    }

    // Draw ZR button (outlined, smaller text)
    static float DrawZRButton(ImDrawList* draw_list, float x, float center_y,
                              const char* label, ImU32 label_color, bool active) {
        const bool is_dark = GUI::IsCurrentThemeDark();
        const ImU32 color_outline = active ? GUI::GetAccentColorU32() : (is_dark ? IM_COL32(120, 120, 120, 255) : IM_COL32(100, 100, 105, 255));
        
        float btn_y = center_y - ZR_HEIGHT * 0.5f;
        
        // ZR button shape (chamfer on top-right)
        ImVec2 points[5] = {
            ImVec2(x, btn_y),
            ImVec2(x + ZR_WIDTH - ZR_CHAMFER, btn_y),
            ImVec2(x + ZR_WIDTH, btn_y + ZR_CHAMFER),
            ImVec2(x + ZR_WIDTH, btn_y + ZR_HEIGHT),
            ImVec2(x, btn_y + ZR_HEIGHT)
        };
        draw_list->AddPolyline(points, 5, color_outline, ImDrawFlags_Closed, 2.0f);
        
        // Draw "ZR" text centered (smaller font)
        const char* zr_text = "ZR";
        const float zr_font_size = ImGui::GetFontSize() * 0.75f;
        ImVec2 zr_text_size = ImGui::GetFont()->CalcTextSizeA(zr_font_size, FLT_MAX, 0.0f, zr_text);
        ImVec2 zr_text_pos(x + (ZR_WIDTH - zr_text_size.x) * 0.5f, center_y - zr_text_size.y * 0.5f);
        draw_list->AddText(ImGui::GetFont(), zr_font_size, zr_text_pos, color_outline, zr_text);
        
        // Draw label
        float label_x = x + ZR_WIDTH + 6.0f;
        draw_list->AddText(ImVec2(label_x, center_y - ImGui::GetTextLineHeight() * 0.5f), label_color, label);
        
        return ZR_WIDTH + 6.0f + ImGui::CalcTextSize(label).x;
    }

    // Draw DPad Up arrow
    static float DrawDPadUp(ImDrawList* draw_list, float x, float center_y,
                            const char* label, ImU32 label_color) {
        const bool is_dark = GUI::IsCurrentThemeDark();
        const ImU32 color_dpad = is_dark ? IM_COL32(80, 80, 80, 255) : IM_COL32(140, 140, 145, 255);
        
        float arrow_x = x + DPAD_SIZE;
        ImVec2 p1(arrow_x, center_y - DPAD_SIZE);
        ImVec2 p2(arrow_x - DPAD_SIZE, center_y + DPAD_SIZE * 0.5f);
        ImVec2 p3(arrow_x + DPAD_SIZE, center_y + DPAD_SIZE * 0.5f);
        draw_list->AddTriangleFilled(p1, p2, p3, color_dpad);
        
        // Draw label
        float label_x = x + DPAD_SIZE * 2 + 6.0f;
        draw_list->AddText(ImVec2(label_x, center_y - ImGui::GetTextLineHeight() * 0.5f), label_color, label);
        
        return DPAD_SIZE * 2 + 6.0f + ImGui::CalcTextSize(label).x;
    }

    // Draw DPad Down arrow
    static float DrawDPadDown(ImDrawList* draw_list, float x, float center_y,
                              const char* label, ImU32 label_color) {
        const bool is_dark = GUI::IsCurrentThemeDark();
        const ImU32 color_dpad = is_dark ? IM_COL32(80, 80, 80, 255) : IM_COL32(140, 140, 145, 255);
        
        float arrow_x = x + DPAD_SIZE;
        ImVec2 p1(arrow_x, center_y + DPAD_SIZE);
        ImVec2 p2(arrow_x - DPAD_SIZE, center_y - DPAD_SIZE * 0.5f);
        ImVec2 p3(arrow_x + DPAD_SIZE, center_y - DPAD_SIZE * 0.5f);
        draw_list->AddTriangleFilled(p1, p2, p3, color_dpad);
        
        // Draw label
        float label_x = x + DPAD_SIZE * 2 + 6.0f;
        draw_list->AddText(ImVec2(label_x, center_y - ImGui::GetTextLineHeight() * 0.5f), label_color, label);
        
        return DPAD_SIZE * 2 + 6.0f + ImGui::CalcTextSize(label).x;
    }

    // Draw Right Stick with up/down arrows inside
    static float DrawRightStick(ImDrawList* draw_list, float x, float center_y,
                                const char* label, ImU32 label_color) {
        const bool is_dark = GUI::IsCurrentThemeDark();
        const ImU32 color_stick = is_dark ? IM_COL32(80, 80, 80, 255) : IM_COL32(140, 140, 145, 255);
        
        ImVec2 stick_center(x + STICK_RADIUS, center_y);
        
        // Draw stick circle outline
        draw_list->AddCircle(stick_center, STICK_RADIUS, color_stick, 16, 2.0f);
        
        // Draw small up/down arrows inside
        const float arrow_size = 4.0f;
        // Up arrow
        draw_list->AddTriangleFilled(
            ImVec2(stick_center.x, stick_center.y - arrow_size - 1.0f),
            ImVec2(stick_center.x - arrow_size * 0.6f, stick_center.y - 1.0f),
            ImVec2(stick_center.x + arrow_size * 0.6f, stick_center.y - 1.0f),
            color_stick
        );
        // Down arrow
        draw_list->AddTriangleFilled(
            ImVec2(stick_center.x, stick_center.y + arrow_size + 1.0f),
            ImVec2(stick_center.x - arrow_size * 0.6f, stick_center.y + 1.0f),
            ImVec2(stick_center.x + arrow_size * 0.6f, stick_center.y + 1.0f),
            color_stick
        );
        
        // Draw label
        float label_x = x + STICK_RADIUS * 2 + 6.0f;
        draw_list->AddText(ImVec2(label_x, center_y - ImGui::GetTextLineHeight() * 0.5f), label_color, label);
        
        return STICK_RADIUS * 2 + 6.0f + ImGui::CalcTextSize(label).x;
    }

    // Calculate width of a single hint item
    static float CalcHintWidth(const HintItem& item, float button_radius, float hint_spacing) {
        float base_width = 0.0f;
        
        switch (item.type) {
            case ButtonType::Minus:
            case ButtonType::Plus:
            case ButtonType::CircleA:
            case ButtonType::CircleB:
            case ButtonType::CircleX:
            case ButtonType::CircleY:
                base_width = button_radius * 2 + 6.0f + ImGui::CalcTextSize(item.label).x;
                break;
            case ButtonType::ShoulderL:
            case ButtonType::ShoulderR:
                base_width = SHOULDER_WIDTH + 6.0f + ImGui::CalcTextSize(item.label).x;
                break;
            case ButtonType::ShoulderZR:
                base_width = ZR_WIDTH + 6.0f + ImGui::CalcTextSize(item.label).x;
                break;
            case ButtonType::DPadUp:
            case ButtonType::DPadDown:
                base_width = DPAD_SIZE * 2 + 6.0f + ImGui::CalcTextSize(item.label).x;
                break;
            case ButtonType::RightStick:
                base_width = STICK_RADIUS * 2 + 6.0f + ImGui::CalcTextSize(item.label).x;
                break;
        }
        
        return base_width;
    }

    // Get button color based on type
    static ImU32 GetButtonColor(ButtonType type) {
        switch (type) {
            case ButtonType::CircleA: return GUI::GetButtonColorA();
            case ButtonType::CircleB: return GUI::GetButtonColorB();
            case ButtonType::CircleX: return GUI::GetButtonColorX();
            case ButtonType::CircleY: return GUI::GetButtonColorY();
            case ButtonType::Plus:    return GUI::GetButtonColorPlus();
            case ButtonType::Minus:   return GUI::GetButtonColorMinus();
            default: return IM_COL32(128, 128, 128, 255);
        }
    }

    // Get button letter based on type
    static const char* GetButtonLetter(ButtonType type) {
        switch (type) {
            case ButtonType::CircleA: return "A";
            case ButtonType::CircleB: return "B";
            case ButtonType::CircleX: return "X";
            case ButtonType::CircleY: return "Y";
            case ButtonType::Plus:    return "+";
            case ButtonType::Minus:   return "-";
            default: return "";
        }
    }

    // Draw a single hint item and return width consumed
    static float DrawHintItem(ImDrawList* draw_list, const HintItem& item, float x, float center_y,
                              float button_radius, ImU32 label_color) {
        const ImU32 text_color = GUI::GetButtonTextColor();
        
        switch (item.type) {
            case ButtonType::Minus:
                return DrawMinusButton(draw_list, x, center_y, button_radius, item.label, label_color);
                
            case ButtonType::Plus:
                return DrawPlusButton(draw_list, x, center_y, button_radius, item.label, label_color, item.active);
                
            case ButtonType::CircleA:
            case ButtonType::CircleB:
            case ButtonType::CircleX:
            case ButtonType::CircleY:
                return DrawCircleButton(draw_list, x, center_y, button_radius,
                                        GetButtonLetter(item.type), GetButtonColor(item.type),
                                        text_color, item.label, label_color, item.active);
                
            case ButtonType::ShoulderL:
                return DrawShoulderL(draw_list, x, center_y, item.label, label_color);
                
            case ButtonType::ShoulderR:
                return DrawShoulderR(draw_list, x, center_y, item.label, label_color);
                
            case ButtonType::ShoulderZR:
                return DrawZRButton(draw_list, x, center_y, item.label, label_color, item.active);
                
            case ButtonType::DPadUp:
                return DrawDPadUp(draw_list, x, center_y, item.label, label_color);
                
            case ButtonType::DPadDown:
                return DrawDPadDown(draw_list, x, center_y, item.label, label_color);
                
            case ButtonType::RightStick:
                return DrawRightStick(draw_list, x, center_y, item.label, label_color);
        }
        
        return 0.0f;
    }

    void Draw(const Config& config,
              const std::vector<HintItem>& left_items,
              const std::vector<HintItem>& right_items) {
        
        // Get draw list based on config
        ImDrawList* draw_list = config.use_foreground_draw_list 
            ? ImGui::GetForegroundDrawList() 
            : ImGui::GetWindowDrawList();
        
        // Get display dimensions
        const float display_w = static_cast<float>(GUI::display_width);
        const float display_h = static_cast<float>(GUI::display_height);
        
        // Calculate bar position
        float bar_y, center_y, left_x, right_edge;
        
        if (config.use_foreground_draw_list) {
            // Overlay mode: bar at absolute bottom of screen
            bar_y = display_h - config.bar_height;
            center_y = bar_y + config.bar_height * 0.5f;
            left_x = 20.0f;
            right_edge = display_w - 20.0f;
        } else {
            // Window mode: bar at cursor position within window
            ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
            bar_y = cursor_pos.y;
            center_y = cursor_pos.y + config.bar_height * 0.5f;
            left_x = cursor_pos.x + 10.0f;
            right_edge = cursor_pos.x + ImGui::GetContentRegionAvail().x - 10.0f;
        }
        
        // Theme-aware colors
        const bool is_dark = GUI::IsCurrentThemeDark();
        const ImU32 label_color = GUI::GetThemeLabelColor();
        
        // Draw background bar if requested
        if (config.draw_background) {
            const ImU32 bar_bg = is_dark ? IM_COL32(0, 0, 0, 180) : IM_COL32(240, 240, 245, 220);
            if (config.use_foreground_draw_list) {
                draw_list->AddRectFilled(ImVec2(0, bar_y), ImVec2(display_w, display_h), bar_bg);
            } else {
                ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
                draw_list->AddRectFilled(
                    ImVec2(cursor_pos.x, bar_y), 
                    ImVec2(cursor_pos.x + ImGui::GetContentRegionAvail().x, bar_y + config.bar_height), 
                    bar_bg
                );
            }
        }
        
        // Draw left-aligned items
        float x_offset = left_x;
        for (const auto& item : left_items) {
            float width = DrawHintItem(draw_list, item, x_offset, center_y, config.button_radius, label_color);
            x_offset += width + config.hint_spacing;
        }
        
        // Calculate total width of right-aligned items
        float total_right_width = 0.0f;
        for (size_t i = 0; i < right_items.size(); i++) {
            total_right_width += CalcHintWidth(right_items[i], config.button_radius, config.hint_spacing);
            if (i < right_items.size() - 1) {
                total_right_width += config.hint_spacing;
            }
        }
        
        // Draw right-aligned items
        x_offset = right_edge - total_right_width;
        for (const auto& item : right_items) {
            float width = DrawHintItem(draw_list, item, x_offset, center_y, config.button_radius, label_color);
            x_offset += width + config.hint_spacing;
        }
    }
}
