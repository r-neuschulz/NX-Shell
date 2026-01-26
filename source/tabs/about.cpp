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

// 3rd party dependency versions - provided by CMake from dependencies.json
// These are defined as compile definitions: DEP_IMGUI_VERSION, DEP_LIBUSBHSFS_VERSION, etc.

namespace Tabs {
    void RequestAboutFocus(void) {
        need_focus_about = true;
    }

    void About(App &app, int &current_tab, int &active_tab) {
        ImGuiTabItemFlags flags = (current_tab == 2) ? ImGuiTabItemFlags_SetSelected : 0;
        if (current_tab == 2) current_tab = -1; // Reset after applying
        
        if (ImGui::BeginTabItem(strings[app.config.Lang()][Lang::TabAbout], nullptr, flags)) {
            active_tab = 2;  // Update active tab when this tab is visible
            const int lang = app.config.Lang();
            
            // Reserve space for button bar at the bottom
            const float button_bar_height = 40.0f;
            float available_height = ImGui::GetContentRegionAvail().y - button_bar_height;
            
            // Begin scrollable child region for about content
            ImGui::BeginChild("AboutContent", ImVec2(0, available_height), false);
            
            // ============================================================
            // UPDATE SECTION
            // ============================================================
            Internal::Indent(strings[lang][Lang::SettingsUpdateTitle]);
            
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
            
            Internal::Separator();
            
            // ============================================================
            // ABOUT SECTION
            // ============================================================
            Internal::Indent(strings[lang][Lang::SettingsAboutTitle]);
            
            ImGui::Text("NX-Shell %s: v%d.%d.%d", strings[lang][Lang::SettingsAboutVersion], VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO);
            ImGui::Dummy(ImVec2(0.0f, 5.0f));
            ImGui::Text("%s: Joel16", strings[lang][Lang::SettingsAboutAuthor]);
            ImGui::Dummy(ImVec2(0.0f, 5.0f));
            ImGui::Text("%s: Preetisketch", strings[lang][Lang::SettingsAboutBanner]);
            ImGui::Dummy(ImVec2(0.0f, 5.0f));
            ImGui::Text("%s: GPL-2.0", strings[lang][Lang::SettingsAboutLicense]);
            
            Internal::Separator();
            
            // ============================================================
            // 3RD PARTY COMPONENTS SECTION
            // ============================================================
            Internal::Indent(strings[lang][Lang::Settings3rdPartyTitle]);
            
            // Core libraries (versions from dependencies.json via CMake)
            ImGui::Text("Dear ImGui %s", DEP_IMGUI_VERSION);
            ImGui::Dummy(ImVec2(0.0f, 3.0f));
            ImGui::Text("libusbhsfs v%s", DEP_LIBUSBHSFS_VERSION);
            ImGui::Dummy(ImVec2(0.0f, 3.0f));
            ImGui::Text("libnsbmp (%s)", DEP_LIBNSBMP_COMMIT);
            ImGui::Dummy(ImVec2(0.0f, 3.0f));
            ImGui::Text("stb (%s)", DEP_STB_COMMIT);
            ImGui::Dummy(ImVec2(0.0f, 10.0f));
            
            // devkitPro libraries (greyed out as secondary info)
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::Text("libcurl");
            ImGui::SameLine(0, 15); ImGui::Text("libpng");
            ImGui::SameLine(0, 15); ImGui::Text("libjpeg-turbo");
            ImGui::Text("libwebp");
            ImGui::SameLine(0, 15); ImGui::Text("libgif");
            ImGui::SameLine(0, 15); ImGui::Text("freetype2");
            ImGui::Text("jansson");
            ImGui::SameLine(0, 15); ImGui::Text("minizip");
            ImGui::SameLine(0, 15); ImGui::Text("glad");
            ImGui::PopStyleColor();
            
            Internal::EndSection();
            
            ImGui::EndChild();
            
            // Draw bottom bar with (-) Quit, (A) Select, and (B) Back buttons
            Internal::DrawBottomBar(app.config, {Internal::Button::Quit}, {Internal::Button::Select, Internal::Button::Back});
            
            ImGui::EndTabItem();
        }
        
        if (update_popup)
            Popups::UpdatePopup(app, update_popup, network_status, update_available, tag_name);
    }
}
