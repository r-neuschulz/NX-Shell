#pragma once

// Internal helpers shared across tab implementations
// Not part of the public Tabs interface

#include <initializer_list>
#include <vector>

#include "bottombar.hpp"
#include "config.hpp"
#include "gui.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "language.hpp"
#include "services.hpp"

namespace Tabs {
namespace Internal {

    // Indentation constants
    constexpr float HEADER_INDENT = 10.0f;   // Indent for section headers
    constexpr float CONTENT_INDENT = 10.0f;  // Additional indent for content (total = HEADER + CONTENT)

    // Draw indented section header
    inline void Indent(const std::string &title) {
        ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
        ImGui::Indent(HEADER_INDENT);
        ImGui::TextColored(ImGui::GetStyle().Colors[ImGuiCol_CheckMark], title.c_str());
        ImGui::Indent(CONTENT_INDENT);
        ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
    }

    // End indented section with separator (use between sections)
    inline void Separator(void) {
        ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
        ImGui::Unindent(HEADER_INDENT + CONTENT_INDENT);
        ImGui::Separator();
    }
    
    // End indented section without separator (use for last section)
    inline void EndSection(void) {
        ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
        ImGui::Unindent(HEADER_INDENT + CONTENT_INDENT);
    }

    // ============================================================
    // Semantic Bottom Bar Buttons (wraps BottomBar system)
    // ============================================================
    
    // Semantic button types that automatically resolve to the correct
    // physical button appearance and language string
    enum class Button {
        Quit,       // (-) Hold to exit - special with progress arc
        Back,       // (B) Yellow - navigate back
        Confirm,    // (A) Green - confirm/select action
        Select,     // (A) Green - select action (same as confirm but different label)
        Cancel,     // (X) Blue - cancel operation
        Options,    // (+) Gray - open options/menu
        Details,    // (Y) Green - toggle details view
    };
    
    // Convert semantic Button to BottomBar::HintItem
    inline BottomBar::HintItem ButtonToHintItem(ConfigService &cfg_svc, Button btn) {
        const int lang = cfg_svc.Lang();
        
        switch (btn) {
            case Button::Quit:
                return {BottomBar::ButtonType::Minus, strings[lang][Lang::HintExit]};
            case Button::Back:
                return {BottomBar::ButtonType::CircleB, strings[lang][Lang::HintBack]};
            case Button::Confirm:
                return {BottomBar::ButtonType::CircleA, strings[lang][Lang::HintConfirm]};
            case Button::Select:
                return {BottomBar::ButtonType::CircleA, strings[lang][Lang::HintSelect]};
            case Button::Cancel:
                return {BottomBar::ButtonType::CircleX, strings[lang][Lang::HintCancel]};
            case Button::Options:
                return {BottomBar::ButtonType::Plus, strings[lang][Lang::HintOptions]};
            case Button::Details:
                return {BottomBar::ButtonType::CircleY, strings[lang][Lang::HintDetails]};
            default:
                return {BottomBar::ButtonType::CircleB, strings[lang][Lang::HintBack]};
        }
    }
    
    // Draw the bottom bar with configurable buttons
    // left_buttons:  Buttons aligned to the left edge (e.g., Quit)
    // right_buttons: Buttons aligned to the right edge (e.g., Back, Confirm)
    //                Listed in display order (leftmost to rightmost)
    //
    // Example usage:
    //   DrawBottomBar(app.config, {Button::Quit}, {Button::Back});                    // Settings/About
    //   DrawBottomBar(app.config, {Button::Quit}, {Button::Confirm, Button::Back});   // With confirm
    //   DrawBottomBar(app.config, {Button::Quit}, {Button::Details, Button::Options, Button::Back}); // File browser
    //
    inline void DrawBottomBar(ConfigService &cfg_svc, std::initializer_list<Button> left_buttons,
                              std::initializer_list<Button> right_buttons) {
        // Convert semantic buttons to BottomBar items
        std::vector<BottomBar::HintItem> left_items;
        std::vector<BottomBar::HintItem> right_items;
        
        for (Button btn : left_buttons) {
            left_items.push_back(ButtonToHintItem(cfg_svc, btn));
        }
        
        for (Button btn : right_buttons) {
            right_items.push_back(ButtonToHintItem(cfg_svc, btn));
        }
        
        // Configure for window-embedded mode (not overlay)
        BottomBar::Config config;
        config.use_foreground_draw_list = false;
        config.draw_background = false;
        config.bar_height = 40.0f;
        config.button_radius = 12.0f;
        config.hint_spacing = 20.0f;
        
        BottomBar::Draw(cfg_svc, config, left_items, right_items);
    }
    
    // Convenience overload: Default bottom bar with just Quit and Back
    inline void DrawBottomBar(ConfigService &cfg_svc) {
        DrawBottomBar(cfg_svc, {Button::Quit}, {Button::Back});
    }

} // namespace Internal
} // namespace Tabs
