#include "config.hpp"
#include "gui.hpp"
#include "imgui.h"
#include "language.hpp"
#include "popups.hpp"
#include "windows.hpp"

namespace Popups {
    // Returns: 0 = cancelled/pending, 1 = open as text, 2 = open as hex
    int OpenModePopup(bool &show) {
        static int result = 0;
        
        if (!show) {
            result = 0;
            return 0;
        }
        
        const int lang = Config::GetLang();
        Popups::SetupPopup(strings[lang][Lang::HexModeOpenTitle]);
        
        if (ImGui::BeginPopupModal(strings[lang][Lang::HexModeOpenTitle], nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("%s", strings[lang][Lang::HexModeOpenMessage]);
            
            ImGui::Dummy(ImVec2(0.0f, 10.0f)); // Spacing
            
            // Center the buttons
            float button_width = 150.0f;
            float spacing = 15.0f;
            float total_width = button_width * 2 + spacing;
            float start_x = (ImGui::GetWindowWidth() - total_width) * 0.5f;
            ImGui::SetCursorPosX(start_x);
            
            if (ImGui::Button(strings[lang][Lang::HexModeOpenAsText], ImVec2(button_width, 0))) {
                result = 1;  // Open as text
                show = false;
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::SameLine(0.0f, spacing);
            
            if (ImGui::Button(strings[lang][Lang::HexModeOpenAsHex], ImVec2(button_width, 0))) {
                result = 2;  // Open as hex
                show = false;
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
            
            // Cancel button (centered)
            float cancel_width = 100.0f;
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - cancel_width) * 0.5f);
            if (ImGui::Button(strings[lang][Lang::ButtonCancel], ImVec2(cancel_width, 0))) {
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
