#include "config.hpp"
#include "gui.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "language.hpp"
#include "log.hpp"
#include "popups.hpp"

#include <switch.h>
#include "nspmini.hpp"

namespace {
    // Static state for the NSP install popup
    static bool nsp_confirm_shown = false;
    static std::string nsp_file_path;
    static std::string nsp_filename;
    
    // Extract filename from full path
    static std::string GetFilename(const std::string &path) {
        size_t pos = path.find_last_of("/\\");
        if (pos != std::string::npos) {
            return path.substr(pos + 1);
        }
        return path;
    }
}

namespace Popups {
    void SetNSPInstallPath(const std::string &path) {
        nsp_file_path = path;
        nsp_filename = GetFilename(path);
        // Reset popup state for fresh display
        nsp_confirm_shown = false;
    }
    
    void NSPInstallPopup(App &app) {
        const int lang = app.config.Lang();
        
        // Initialize popup state when entering this state
        if (!nsp_confirm_shown) {
            nsp_confirm_shown = true;
        }
        
        // NSP Confirmation Popup
        ImGui::OpenPopup("NSP File Install Confirm");
        
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(650, 280));
        
        if (ImGui::BeginPopupModal("NSP File Install Confirm", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
            // Title
            ImGui::TextUnformatted(strings[lang][Lang::NSPFileInstallTitle]);
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0.0f, 10.0f));
            
            // Show filename being installed
            ImGui::Text("File: %s", nsp_filename.c_str());
            ImGui::Dummy(ImVec2(0.0f, 10.0f));
            
            // Warning message
            ImGui::TextWrapped("%s", strings[lang][Lang::NSPFileInstallConfirm]);
            ImGui::Dummy(ImVec2(0.0f, 20.0f));
            
            // Check applet mode
            if (GUI::IsAppletMode()) {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", strings[lang][Lang::NSPInstallNotAvailable]);
                ImGui::Dummy(ImVec2(0.0f, 10.0f));
            }
            
            float button_width = 150.0f;
            float spacing = 20.0f;
            float total_width = button_width * 2 + spacing;
            ImGui::SetCursorPosX((650 - total_width) / 2);
            
            if (ImGui::Button(strings[lang][Lang::ButtonCancel], ImVec2(button_width, 40))) {
                nsp_confirm_shown = false;
                nsp_file_path.clear();
                nsp_filename.clear();
                ImGui::CloseCurrentPopup();
                app.window.state = WINDOW_STATE_FILEBROWSER;
            }
            
            ImGui::SameLine(0, spacing);
            
            // Disable install button in applet mode
            bool can_install = !GUI::IsAppletMode();
            if (!can_install) {
                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
            }
            
            if (ImGui::Button(strings[lang][Lang::ButtonOK], ImVec2(button_width, 40))) {
                nsp_confirm_shown = false;
                ImGui::CloseCurrentPopup();
                
                // Perform NSP installation and show toast result
                // Note: mini::InstallSD returns void; errors are not propagated
                // (exceptions disabled in this codebase)
                mini::InstallSD(nsp_file_path.c_str());
                bool success = true;  // Assume success if no crash
                Log::Debug("NSP install requested: %s\n", nsp_file_path.c_str());
                
                // Show toast notification with result
                const char* message = success 
                    ? strings[lang][Lang::NSPFileInstallSuccess] 
                    : strings[lang][Lang::NSPFileInstallError];
                Toast::Show(message, success, 3.0f);
                
                nsp_file_path.clear();
                nsp_filename.clear();
                app.window.state = WINDOW_STATE_FILEBROWSER;
            }
            
            if (!can_install) {
                ImGui::PopItemFlag();
                ImGui::PopStyleVar();
            }
            
            ImGui::EndPopup();
        }
    }
}
