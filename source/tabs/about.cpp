#include "config.hpp"
#include "gui.hpp"
#include "imgui.h"
#include "language.hpp"
#include "log.hpp"
#include "net.hpp"
#include "popups.hpp"
#include "tabs.hpp"
#include "tabs_internal.hpp"
#include "utils.hpp"

static bool need_focus_about = false;
static bool update_popup = false, network_status = false, update_available = false;
static std::string tag_name = std::string();

namespace Tabs {
    void RequestAboutFocus(void) {
        need_focus_about = true;
    }

    void About(WindowData &data, int &current_tab, int &active_tab) {
        ImGuiTabItemFlags flags = (current_tab == 2) ? ImGuiTabItemFlags_SetSelected : 0;
        if (current_tab == 2) current_tab = -1; // Reset after applying
        
        if (ImGui::BeginTabItem(strings[Config::GetLang()][Lang::TabAbout], nullptr, flags)) {
            active_tab = 2;  // Update active tab when this tab is visible
            const int lang = Config::GetLang();
            
            // Reserve space for button bar at the bottom
            const float button_bar_height = 40.0f;
            float available_height = ImGui::GetContentRegionAvail().y - button_bar_height;
            
            // Begin scrollable child region for about content
            ImGui::BeginChild("AboutContent", ImVec2(0, available_height), false);
            
            ImGui::Indent(10.f);
            Internal::Indent(strings[lang][Lang::SettingsAboutTitle]);
            
            ImGui::Text("NX-Shell %s: v%d.%d.%d", strings[lang][Lang::SettingsAboutVersion], VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO);
            ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
            ImGui::Text("Dear ImGui %s: %s", strings[lang][Lang::SettingsAboutVersion], ImGui::GetVersion());
            ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
            ImGui::Text("%s: Joel16", strings[lang][Lang::SettingsAboutAuthor]);
            ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
            ImGui::Text("%s: Preetisketch", strings[lang][Lang::SettingsAboutBanner]);
            ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
            ImGui::Text("%s: GPL-2.0", strings[lang][Lang::SettingsAboutLicense]);
            ImGui::Dummy(ImVec2(0.0f, 15.0f));
            
            // Unindent before "Check for Updates" button
            ImGui::Unindent(10.f);
            
            // Focus "Check for Updates" button when tab is switched via L/R
            if (need_focus_about) {
                ImGui::SetKeyboardFocusHere();
                need_focus_about = false;
            }
            
            // Disable update check in applet mode (limited memory/network access)
            if (GUI::IsAppletMode()) {
                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
            }
            
            const char* update_text = strings[lang][Lang::SettingsCheckForUpdates];
            float update_button_width = std::max(250.0f, ImGui::CalcTextSize(update_text).x + 40.0f);
            if (ImGui::Button(update_text, ImVec2(update_button_width, 50))) {
                tag_name = Net::GetLatestReleaseJSON();
                network_status = Net::GetNetworkStatus();
                update_available = Net::GetAvailableUpdate(tag_name);
                update_popup = true;
            }
            
            if (GUI::IsAppletMode()) {
                ImGui::PopItemFlag();
                ImGui::PopStyleVar();
            }
            
            ImGui::EndChild();
            
            // Draw bottom bar with (-) Quit, (A) Select, and (B) Back buttons
            Internal::DrawBottomBar({Internal::Button::Quit}, {Internal::Button::Select, Internal::Button::Back});
            
            ImGui::EndTabItem();
        }
        
        if (update_popup)
            Popups::UpdatePopup(update_popup, network_status, update_available, tag_name);
    }
}
