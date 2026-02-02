#include "config.hpp"
#include "gui.hpp"
#include "imgui.h"
#include "language.hpp"
#include "popups.hpp"
#include "windows.hpp"

namespace Popups {
    // Returns: 0 = cancelled/pending, 1 = open as text, 2 = open as hex
    int OpenModePopup(App &app, bool &show) {
        static int result = 0;
        
        if (!show) {
            result = 0;
            return 0;
        }
        
        const int lang = app.config.Lang();
        Popups::SetupPopup(app, strings[lang][Lang::HexModeOpenTitle]);
        
        if (ImGui::BeginPopupModal(strings[lang][Lang::HexModeOpenTitle], nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar)) {
            ImGui::Text("%s", strings[lang][Lang::HexModeOpenMessage]);
            
            ImGui::Dummy(ImVec2(0.0f, 10.0f)); // Spacing
            
            // Center the buttons
            float button_width = 120.0f;
            float spacing_btn = 15.0f;
            float total_width = button_width * 2 + spacing_btn;
            float start_x = (ImGui::GetWindowWidth() - total_width) * 0.5f;
            ImGui::SetCursorPosX(start_x);
            
            if (ImGui::Button(strings[lang][Lang::HexModeOpenAsText], ImVec2(button_width, 36))) {
                result = 1;  // Open as text
                show = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SetItemDefaultFocus();
            
            ImGui::SameLine(0.0f, spacing_btn);
            
            if (ImGui::Button(strings[lang][Lang::HexModeOpenAsHex], ImVec2(button_width, 36))) {
                result = 2;  // Open as hex
                show = false;
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
            
            // Cancel button (centered)
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - button_width) * 0.5f);
            if (ImGui::Button(strings[lang][Lang::ButtonCancel], ImVec2(button_width, 36))) {
                result = 0;  // Cancelled
                show = false;
                ImGui::CloseCurrentPopup();
            }
        }
        
        Popups::ExitPopup();
        
        int ret = result;
        if (!show) {
            result = 0;  // Reset for next time
        }
        return ret;
    }
}
