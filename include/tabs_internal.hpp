#pragma once

// Internal helpers shared across tab implementations
// Not part of the public Tabs interface

#include <cmath>
#include <initializer_list>

#include "config.hpp"
#include "gui.hpp"
#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"
#include "language.hpp"

namespace Tabs {
namespace Internal {

    // Draw indented section header
    inline void Indent(const std::string &title) {
        ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
        ImGui::TextColored(ImGui::GetStyle().Colors[ImGuiCol_CheckMark], title.c_str());
        ImGui::Indent(20.f);
        ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
    }

    // End indented section with separator
    inline void Separator(void) {
        ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
        ImGui::Unindent();
        ImGui::Separator();
    }

    // ============================================================
    // Generalized Bottom Bar System
    // ============================================================
    
    // Available buttons for the bottom bar
    enum class Button {
        Quit,       // (-) Hold to exit - special with progress arc
        Back,       // (B) Yellow - navigate back
        Confirm,    // (A) Green - confirm/select action
        Select,     // (A) Green - select action (same as confirm but different label)
        Cancel,     // (X) Blue - cancel operation
        Options,    // (+) Gray - open options/menu
        Details,    // (Y) Green - toggle details view
    };
    
    // Nintendo Switch button colors - use runtime functions for style-aware colors
    // These helpers provide the appropriate color based on cfg.button_style:
    // - ButtonStyle_Colored: Traditional Nintendo Switch colored buttons
    // - ButtonStyle_Mono: Theme-aware monochrome buttons
    namespace ButtonColors {
        // Runtime color getters - use GUI:: functions for style-aware colors
        inline ImU32 A() { return GUI::GetButtonColorA(); }
        inline ImU32 B() { return GUI::GetButtonColorB(); }
        inline ImU32 X() { return GUI::GetButtonColorX(); }
        inline ImU32 Y() { return GUI::GetButtonColorY(); }
        inline ImU32 Plus() { return GUI::GetButtonColorPlus(); }
        inline ImU32 Minus() { return GUI::GetButtonColorMinus(); }
        inline ImU32 Text() { return GUI::GetButtonTextColor(); }
        // Label color is theme-dependent - use GUI::GetThemeLabelColor() at runtime
        // Progress color uses accent - call GUI::GetAccentColorU32() at runtime
    }
    
    // Get the display properties for a button
    struct ButtonInfo {
        const char* letter;       // Letter/symbol to show (nullptr for special shapes)
        ImU32 color;              // Background color
        Lang::StringID label_id;  // Language string ID for the label
        bool is_minus;            // Special minus button with hold-to-close
        bool is_plus;             // Special plus button shape
    };
    
    inline ButtonInfo GetButtonInfo(Button btn) {
        switch (btn) {
            case Button::Quit:
                return { nullptr, ButtonColors::Minus(), Lang::HintExit, true, false };
            case Button::Back:
                return { "B", ButtonColors::B(), Lang::HintBack, false, false };
            case Button::Confirm:
                return { "A", ButtonColors::A(), Lang::HintConfirm, false, false };
            case Button::Select:
                return { "A", ButtonColors::A(), Lang::HintSelect, false, false };
            case Button::Cancel:
                return { "X", ButtonColors::X(), Lang::HintCancel, false, false };
            case Button::Options:
                return { nullptr, ButtonColors::Plus(), Lang::HintOptions, false, true };
            case Button::Details:
                return { "Y", ButtonColors::Y(), Lang::HintDetails, false, false };
            default:
                return { "?", IM_COL32(120, 120, 120, 255), Lang::HintBack, false, false };
        }
    }
    
    // Draw a single button and return its total width
    inline float DrawButton(ImDrawList* draw_list, float x, float center_y, 
                            Button btn, float button_radius, bool right_to_left = false) {
        const int lang = Config::GetLang();
        ButtonInfo info = GetButtonInfo(btn);
        const char* label = strings[lang][info.label_id];
        ImVec2 label_size = ImGui::CalcTextSize(label);
        
        // Calculate total width: button + spacing + label
        float total_width = button_radius * 2 + 6.0f + label_size.x;
        
        // Adjust x position if drawing right-to-left
        float button_x = right_to_left ? (x - total_width + button_radius) : (x + button_radius);
        ImVec2 center(button_x, center_y);
        
        if (info.is_minus) {
            // Special minus button with hold-to-close progress
            float hold_progress = 0.0f;
            bool is_holding = GUI::IsHoldingToClose(hold_progress);
            
            // Draw background circle
            draw_list->AddCircleFilled(center, button_radius, info.color, 24);
            
            // Draw progress arc when holding
            if (is_holding && hold_progress > 0.0f) {
                const float arc_radius = button_radius + 3.0f;
                const float start_angle = -IM_PI * 0.5f;
                const float end_angle = start_angle + (hold_progress * IM_PI * 2.0f);
                
                const int num_segments = static_cast<int>(24 * hold_progress) + 1;
                for (int i = 0; i < num_segments; i++) {
                    float a1 = start_angle + (end_angle - start_angle) * (static_cast<float>(i) / num_segments);
                    float a2 = start_angle + (end_angle - start_angle) * (static_cast<float>(i + 1) / num_segments);
                    ImVec2 p1(center.x + std::cosf(a1) * arc_radius, center.y + std::sinf(a1) * arc_radius);
                    ImVec2 p2(center.x + std::cosf(a2) * arc_radius, center.y + std::sinf(a2) * arc_radius);
                    draw_list->AddLine(p1, p2, GUI::GetAccentColorU32(), 3.0f);
                }
            }
            
            // Draw minus sign
            const float minus_width = button_radius * 0.8f;
            draw_list->AddLine(
                ImVec2(center.x - minus_width, center.y),
                ImVec2(center.x + minus_width, center.y),
                ButtonColors::Text(), 2.0f
            );
            
            // Draw label with highlight when holding
            float label_x = right_to_left ? (button_x + button_radius + 6.0f) : (button_x + button_radius + 6.0f);
            ImU32 label_color = is_holding 
                ? GUI::GetAccentColorU32WithAlpha(200 + static_cast<int>(55 * hold_progress))
                : GUI::GetThemeLabelColor();
            draw_list->AddText(ImVec2(label_x, center_y - ImGui::GetTextLineHeight() * 0.5f), label_color, label);
        }
        else if (info.is_plus) {
            // Special plus button
            draw_list->AddCircleFilled(center, button_radius, info.color, 24);
            
            // Draw plus sign
            const float plus_width = button_radius * 0.8f;
            draw_list->AddLine(
                ImVec2(center.x - plus_width, center.y),
                ImVec2(center.x + plus_width, center.y),
                ButtonColors::Text(), 2.0f
            );
            draw_list->AddLine(
                ImVec2(center.x, center.y - plus_width),
                ImVec2(center.x, center.y + plus_width),
                ButtonColors::Text(), 2.0f
            );
            
            // Draw label
            float label_x = right_to_left ? (button_x + button_radius + 6.0f) : (button_x + button_radius + 6.0f);
            draw_list->AddText(ImVec2(label_x, center_y - ImGui::GetTextLineHeight() * 0.5f), GUI::GetThemeLabelColor(), label);
        }
        else {
            // Standard circle button with letter
            draw_list->AddCircleFilled(center, button_radius, info.color, 24);
            
            // Draw letter centered
            ImVec2 text_size = ImGui::CalcTextSize(info.letter);
            ImVec2 text_pos(center.x - text_size.x * 0.5f + 1.0f, center.y - text_size.y * 0.5f);
            draw_list->AddText(text_pos, ButtonColors::Text(), info.letter);
            
            // Draw label
            float label_x = right_to_left ? (button_x + button_radius + 6.0f) : (button_x + button_radius + 6.0f);
            draw_list->AddText(ImVec2(label_x, center_y - ImGui::GetTextLineHeight() * 0.5f), GUI::GetThemeLabelColor(), label);
        }
        
        return total_width;
    }
    
    // Draw the bottom bar with configurable buttons
    // left_buttons:  Buttons aligned to the left edge (e.g., Quit)
    // right_buttons: Buttons aligned to the right edge (e.g., Back, Confirm)
    //                Listed in display order (leftmost to rightmost)
    //
    // Example usage:
    //   DrawBottomBar({Button::Quit}, {Button::Back});                    // Settings/About
    //   DrawBottomBar({Button::Quit}, {Button::Confirm, Button::Back});   // With confirm
    //   DrawBottomBar({Button::Quit}, {Button::Details, Button::Options, Button::Back}); // File browser
    //
    inline void DrawBottomBar(std::initializer_list<Button> left_buttons,
                              std::initializer_list<Button> right_buttons) {
        const float button_bar_height = 40.0f;
        const float button_radius = 12.0f;
        const float padding = 10.0f;
        const float button_spacing = 20.0f;  // Space between buttons
        
        ImDrawList *draw_list = ImGui::GetWindowDrawList();
        ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
        float center_y = cursor_pos.y + (button_bar_height * 0.5f);
        
        // Draw left-aligned buttons
        float left_x = cursor_pos.x + padding;
        for (Button btn : left_buttons) {
            float width = DrawButton(draw_list, left_x, center_y, btn, button_radius, false);
            left_x += width + button_spacing;
        }
        
        // Draw right-aligned buttons (from right edge, going left)
        float right_edge = cursor_pos.x + ImGui::GetContentRegionAvail().x - padding;
        float right_x = right_edge;
        
        // Process in reverse order so rightmost button is at the edge
        auto it = right_buttons.end();
        while (it != right_buttons.begin()) {
            --it;
            Button btn = *it;
            ButtonInfo info = GetButtonInfo(btn);
            const char* label = strings[Config::GetLang()][info.label_id];
            ImVec2 label_size = ImGui::CalcTextSize(label);
            float total_width = button_radius * 2 + 6.0f + label_size.x;
            
            right_x -= total_width;
            DrawButton(draw_list, right_x, center_y, btn, button_radius, false);
            right_x -= button_spacing;
        }
    }
    
    // Convenience overload: Default bottom bar with just Quit and Back
    inline void DrawBottomBar(void) {
        DrawBottomBar({Button::Quit}, {Button::Back});
    }

} // namespace Internal
} // namespace Tabs
