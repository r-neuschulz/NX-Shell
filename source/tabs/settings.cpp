#include "config.hpp"
#include "fs.hpp"
#include "gui.hpp"
#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"
#include "language.hpp"
#include "net.hpp"
#include "popups.hpp"
#include "tabs.hpp"
#include "usb.hpp"

static bool need_focus_settings = false;
static bool need_focus_about = false;

namespace Tabs {
    void RequestSettingsFocus(void) {
        need_focus_settings = true;
    }
    
    void RequestAboutFocus(void) {
        need_focus_about = true;
    }
    
    static bool update_popup = false, network_status = false, update_available = false, unmount_popup = false;
    static std::string tag_name = std::string();

    static void Indent(const std::string &title) {
        ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
        ImGui::TextColored(ImGui::GetStyle().Colors[ImGuiCol_CheckMark], title.c_str());
        ImGui::Indent(20.f);
        ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
    }

    static void Separator(void) {
        ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
        ImGui::Unindent();
        ImGui::Separator();
    }
    
    void Settings(WindowData &data, int &current_tab, int &active_tab) {
        ImGuiTabItemFlags flags = (current_tab == 1) ? ImGuiTabItemFlags_SetSelected : 0;
        if (current_tab == 1) current_tab = -1; // Reset after applying
        
        if (ImGui::BeginTabItem("Settings", nullptr, flags)) {
            active_tab = 1;  // Update active tab when this tab is visible
            const int lang = Config::GetLang();
            
            // Language selector
            ImGui::Indent(10.f);
            Tabs::Indent(strings[lang][Lang::SettingsLanguageTitle]);

            // Only show supported (translated) languages, alphabetically sorted
            struct LanguageOption {
                const char* name;
                int value;  // -1 = Auto, or language index
            };
            
            static const LanguageOption supported_languages[] = {
                {" Auto", LANG_AUTO},
                {" Chinese (Simplified)", 6},
                {" Chinese (Traditional)", 11},
                {" English", 1},
                {" German", 3},
                {" Korean", 7},
                {" Portuguese", 9},
                {" Spanish", 5}
            };
            
            static const int num_languages = sizeof(supported_languages) / sizeof(supported_languages[0]);

            // Find current selection index
            int current_selection = 0;
            for (int i = 0; i < num_languages; i++) {
                if (supported_languages[i].value == cfg.lang) {
                    current_selection = i;
                    break;
                }
            }
            
            // Focus language combo when tab is switched via L/R
            if (need_focus_settings) {
                ImGui::SetKeyboardFocusHere();
                need_focus_settings = false;
            }
            
            ImGui::PushItemWidth(200.f);
            if (ImGui::BeginCombo("##language_combo", supported_languages[current_selection].name)) {
                for (int i = 0; i < num_languages; i++) {
                    const bool is_selected = (current_selection == i);
                    if (ImGui::Selectable(supported_languages[i].name, is_selected)) {
                        cfg.lang = supported_languages[i].value;
                        Config::Save(cfg);
                    }
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();

            Tabs::Separator();

            // USB unmount
            Tabs::Indent(strings[lang][Lang::SettingsUSBTitle]);

            if (!USB::Connected()) {
                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
            }

            if (ImGui::Button(strings[lang][Lang::SettingsUSBUnmount], ImVec2(250, 50)))
                unmount_popup = true;
            
            if (!USB::Connected()) {
                ImGui::PopItemFlag();
                ImGui::PopStyleVar();
            }

            Tabs::Separator();

            // Image filename checkbox
            Tabs::Indent(strings[lang][Lang::SettingsImageViewTitle]);

            if (ImGui::Checkbox(strings[lang][Lang::SettingsImageViewFilenameToggle], std::addressof(cfg.image_filename)))
                Config::Save(cfg);

            Tabs::Separator();

            // Developer Options Checkbox
            Tabs::Indent(strings[lang][Lang::SettingsDevOptsTitle]);

            if (ImGui::Checkbox(strings[lang][Lang::SettingsDevOptsLogsToggle], std::addressof(cfg.dev_options)))
                Config::Save(cfg);

            Tabs::Separator();

            // Multi lang Checkbox
            Tabs::Indent(strings[lang][Lang::SettingsMultiLangTitle]);

            if (ImGui::Checkbox(strings[lang][Lang::SettingsMultiLangLogsToggle], std::addressof(cfg.multi_lang)))
                Config::Save(cfg);

            Tabs::Separator();

            // Display Resolution
            {
                // Build the title with current resolution indicator
                char resolution_title[128];
                std::snprintf(resolution_title, sizeof(resolution_title), "%s (%dp)", 
                    strings[lang][Lang::SettingsResolutionTitle], GUI::display_height);
                Tabs::Indent(resolution_title);

                if (ImGui::RadioButton(strings[lang][Lang::SettingsResolutionAuto], &cfg.resolution_mode, ResolutionMode_Auto))
                    Config::Save(cfg);
                
                ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
                
                if (ImGui::RadioButton(strings[lang][Lang::SettingsResolution1080p], &cfg.resolution_mode, ResolutionMode_1080p))
                    Config::Save(cfg);
                
                ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
                
                if (ImGui::RadioButton(strings[lang][Lang::SettingsResolution720p], &cfg.resolution_mode, ResolutionMode_720p))
                    Config::Save(cfg);
            }

            ImGui::EndTabItem();
        }

        if (unmount_popup)
            Popups::USBPopup(unmount_popup);
    }

    void About(WindowData &data, int &current_tab, int &active_tab) {
        ImGuiTabItemFlags flags = (current_tab == 2) ? ImGuiTabItemFlags_SetSelected : 0;
        if (current_tab == 2) current_tab = -1; // Reset after applying
        
        if (ImGui::BeginTabItem("About", nullptr, flags)) {
            active_tab = 2;  // Update active tab when this tab is visible
            const int lang = Config::GetLang();
            
            ImGui::Indent(10.f);
            Tabs::Indent(strings[lang][Lang::SettingsAboutTitle]);
            
            ImGui::Text("NX-Shell %s: v%d.%d.%d", strings[lang][Lang::SettingsAboutVersion], VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO);
            ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
            ImGui::Text("Dear ImGui %s: %s", strings[lang][Lang::SettingsAboutVersion], ImGui::GetVersion());
            ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
            ImGui::Text("%s: Joel16", strings[lang][Lang::SettingsAboutAuthor]);
            ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
            ImGui::Text("%s: Preetisketch", strings[lang][Lang::SettingsAboutBanner]);
            ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
            
            // Focus "Check for Updates" button when tab is switched via L/R
            if (need_focus_about) {
                ImGui::SetKeyboardFocusHere();
                need_focus_about = false;
            }
            
            if (ImGui::Button(strings[lang][Lang::SettingsCheckForUpdates], ImVec2(250, 50))) {
                tag_name = Net::GetLatestReleaseJSON();
                network_status = Net::GetNetworkStatus();
                update_available = Net::GetAvailableUpdate(tag_name);
                update_popup = true;
            }
            
            ImGui::EndTabItem();
        }
        
        if (update_popup)
            Popups::UpdatePopup(update_popup, network_status, update_available, tag_name);
    }
}
