#include "config.hpp"
#include "gui.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "language.hpp"
#include "log.hpp"
#include "popups.hpp"
#include "nspbuild.hpp"

#include <switch.h>

namespace {
    // Static state for the NRO forwarder popup
    static bool nro_confirm_shown = false;
    static bool nro_building = false;
    static std::string nro_file_path;
    static std::string nro_filename;
    static NSPBuild::NroMetadata nro_metadata;
    
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
    void SetNROForwarderPath(const std::string &path) {
        nro_file_path = path;
        nro_filename = GetFilename(path);
        // Reset popup state for fresh display
        nro_confirm_shown = false;
        nro_building = false;
        // Pre-load metadata
        nro_metadata = NSPBuild::ExtractNroMetadata(path);
    }
    
    void NROForwarderPopup(App &app) {
        const int lang = app.config.Lang();
        
        // Initialize popup state when entering this state
        if (!nro_confirm_shown) {
            nro_confirm_shown = true;
        }
        
        // If building, show progress
        if (nro_building) {
            Popups::SetupPopup(app, strings[lang][Lang::NROForwarderBuilding]);
            
            if (ImGui::BeginPopupModal(strings[lang][Lang::NROForwarderBuilding], nullptr, 
                    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar)) {
                ImGui::TextUnformatted(strings[lang][Lang::NROForwarderBuilding]);
                ImGui::Dummy(ImVec2(0.0f, 10.0f));
                
                // Simple indeterminate progress indicator
                static float progress_anim = 0.0f;
                progress_anim += ImGui::GetIO().DeltaTime * 0.5f;
                if (progress_anim > 1.0f) progress_anim -= 1.0f;
                ImGui::ProgressBar(progress_anim, ImVec2(350.0f, 0.0f), "");
            }
            
            Popups::ExitPopup();
            
            // Process build on next frame (so progress shows)
            static bool should_build = false;
            if (!should_build) {
                should_build = true;
            } else {
                should_build = false;
                nro_building = false;
                
                // Perform actual build
                NSPBuild::NSPBuildConfig config;
                config.nro_path = nro_file_path;
                
                NSPBuild::NSPBuildResult result = NSPBuild::BuildForwarderNSP(config);
                
                if (result.success) {
                    std::string msg = std::string(strings[lang][Lang::NROForwarderSuccess]) + 
                                      "\n\nSaved to: " + result.output_path;
                    Toast::Show(msg.c_str(), true, 5.0f);
                    Log::Debug("NRO Forwarder: Created %s\n", result.output_path.c_str());
                } else {
                    std::string msg = std::string(strings[lang][Lang::NROForwarderError]) + 
                                      "\n\n" + result.error;
                    Toast::Show(msg.c_str(), false, 5.0f);
                    Log::Error("NRO Forwarder: Failed - %s\n", result.error.c_str());
                }
                
                // Clean up and return to file browser
                nro_file_path.clear();
                nro_filename.clear();
                nro_confirm_shown = false;
                nro_metadata = NSPBuild::NroMetadata();
                app.window.state = WINDOW_STATE_FILEBROWSER;
            }
            return;
        }
        
        // NRO Forwarder Confirmation Popup
        Popups::SetupPopup(app, strings[lang][Lang::NROForwarderTitle]);
        
        if (ImGui::BeginPopupModal(strings[lang][Lang::NROForwarderTitle], nullptr, 
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar)) {
            
            // Show NRO info
            ImGui::Text("%s: %s", strings[lang][Lang::PropertiesName], nro_filename.c_str());
            
            if (nro_metadata.valid) {
                ImGui::Text("Title: %s", nro_metadata.name.c_str());
                ImGui::Text("Publisher: %s", nro_metadata.publisher.c_str());
                ImGui::Text("Version: %s", nro_metadata.version.c_str());
                
                // Show generated title ID
                std::string title_id = NSPBuild::GenerateTitleId(nro_file_path);
                ImGui::Text("Title ID: %s", title_id.c_str());
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), 
                    "%s: %s", strings[lang][Lang::NROForwarderError], nro_metadata.error.c_str());
            }
            
            ImGui::Dummy(ImVec2(0.0f, 5.0f));
            
            // Warning/info message
            ImGui::TextWrapped("%s", strings[lang][Lang::NROForwarderConfirm]);
            
            // Check applet mode
            if (GUI::IsAppletMode()) {
                ImGui::Dummy(ImVec2(0.0f, 5.0f));
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), 
                    "%s", strings[lang][Lang::NSPInstallNotAvailable]);
            }
            
            ImGui::Dummy(ImVec2(0.0f, 5.0f));
            
            // Disable build button if invalid metadata
            bool can_build = nro_metadata.valid;
            if (!can_build) {
                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
            }
            
            if (ImGui::Button(strings[lang][Lang::ButtonOK], ImVec2(120, 36))) {
                ImGui::CloseCurrentPopup();
                nro_building = true;
            }
            
            if (can_build) {
                ImGui::SetItemDefaultFocus();
            }
            
            if (!can_build) {
                ImGui::PopItemFlag();
                ImGui::PopStyleVar();
            }
            
            ImGui::SameLine(0, 15);
            
            if (ImGui::Button(strings[lang][Lang::ButtonCancel], ImVec2(120, 36))) {
                nro_confirm_shown = false;
                nro_file_path.clear();
                nro_filename.clear();
                nro_metadata = NSPBuild::NroMetadata();
                ImGui::CloseCurrentPopup();
                app.window.state = WINDOW_STATE_FILEBROWSER;
            }
        }
        
        Popups::ExitPopup();
    }
}
