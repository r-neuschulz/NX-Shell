#include <switch.h>

#include "gui.hpp"
#include "imgui_impl_switch.hpp"
#include "popups.hpp"
#include "windows.hpp"

namespace Popups {
    
    void ProgressBar(App &app, float offset, float size, const std::string &title, const std::string &text) {
        u64 key = ImGui_ImplSwitch_NewFrame();
        ImGui::NewFrame();
        
        Windows::MainWindow(app, key, true);
        Popups::SetupPopup(app, title.c_str());
        
        if (ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("%s", text.c_str());
            ImGui::ProgressBar(size > 0 ? offset / size : 0.0f, ImVec2(400.0f, 0.0f));
        }
        
        Popups::ExitPopup();
        
        ImGui::Render();
        // RenderDrawData handles swapchain present internally
        ImGui_ImplSwitch_RenderDrawData(ImGui::GetDrawData());
    }
}
