#include "gui.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "language.hpp"
#include "log.hpp"
#include "popups.hpp"
#include "nspbuild.hpp"
#include "nspmini.hpp"

#include <switch.h>

namespace {
    static std::string install_path;
    static std::string install_filename;
    static bool is_nsp_file = false;
    static bool installing = false;
}

namespace Popups {
    void SetInstallPath(const std::string &path, bool is_nsp) {
        install_path = path;
        is_nsp_file = is_nsp;
        installing = false;
        
        // Extract filename
        size_t pos = path.find_last_of("/\\");
        install_filename = (pos != std::string::npos) ? path.substr(pos + 1) : path;
    }
    
    void InstallPopup(App &app) {
        const int lang = app.config.Lang();
        
        // If installing, show progress
        if (installing) {
            ImGui::OpenPopup("Installing");
            
            ImVec2 center = ImGui::GetMainViewport()->GetCenter();
            ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(350, 100));
            
            if (ImGui::BeginPopupModal("Installing", nullptr, 
                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
                ImGui::TextUnformatted(strings[lang][Lang::NROForwarderBuilding]);
                ImGui::Dummy(ImVec2(0.0f, 10.0f));
                
                static float progress_anim = 0.0f;
                progress_anim += ImGui::GetIO().DeltaTime * 0.5f;
                if (progress_anim > 1.0f) progress_anim -= 1.0f;
                ImGui::ProgressBar(progress_anim, ImVec2(-1.0f, 0.0f), "");
                
                ImGui::EndPopup();
            }
            
            // Process install on next frame
            static bool should_install = false;
            if (!should_install) {
                should_install = true;
            } else {
                should_install = false;
                installing = false;
                
                bool success = false;
                std::string error_message;
                
                if (is_nsp_file) {
                    // Direct NSP installation
                    try {
                        success = mini::InstallSD(install_path);
                        if (!success) error_message = "Installation failed";
                    } catch (const std::exception& e) {
                        error_message = e.what();
                    } catch (...) {
                        error_message = "Unknown error";
                    }
                } else {
                    // NRO: Build forwarder NSP and install
                    NSPBuild::NSPBuildConfig config;
                    config.nro_path = install_path;
                    
                    NSPBuild::NSPBuildResult result = NSPBuild::BuildForwarderNSP(config);
                    
                    if (result.success) {
                        try {
                            success = mini::InstallSD(result.output_path);
                            if (success) {
                                std::remove(result.output_path.c_str());
                            } else {
                                error_message = "Installation failed";
                            }
                        } catch (const std::exception& e) {
                            error_message = e.what();
                        } catch (...) {
                            error_message = "Unknown error";
                        }
                    } else {
                        error_message = result.error;
                    }
                }
                
                // Show result
                if (success) {
                    Toast::Show(strings[lang][Lang::NSPFileInstallSuccess], true, 3.0f);
                } else {
                    Toast::Show(error_message.empty() ? strings[lang][Lang::NSPFileInstallError] 
                                                      : error_message.c_str(), false, 4.0f);
                }
                
                install_path.clear();
                install_filename.clear();
                app.window.state = WINDOW_STATE_FILEBROWSER;
            }
            return;
        }
        
        // Confirmation popup
        ImGui::OpenPopup("Install");
        
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(420, 180));
        
        if (ImGui::BeginPopupModal("Install", nullptr, 
                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
            
            ImGui::TextWrapped("%s", strings[lang][Lang::NSPFileInstallConfirm]);
            ImGui::Dummy(ImVec2(0.0f, 5.0f));
            
            ImGui::TextDisabled("%s", install_filename.c_str());
            ImGui::Dummy(ImVec2(0.0f, 10.0f));
            
            // Applet mode warning
            if (GUI::IsAppletMode()) {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), 
                    "%s", strings[lang][Lang::NSPInstallNotAvailable]);
                ImGui::Dummy(ImVec2(0.0f, 5.0f));
            }
            
            // Buttons
            float button_width = 120.0f;
            float spacing = 20.0f;
            float total_width = button_width * 2 + spacing;
            ImGui::SetCursorPosX((420 - total_width) / 2);
            
            if (ImGui::Button(strings[lang][Lang::ButtonCancel], ImVec2(button_width, 35))) {
                install_path.clear();
                install_filename.clear();
                ImGui::CloseCurrentPopup();
                app.window.state = WINDOW_STATE_FILEBROWSER;
            }
            
            ImGui::SameLine(0, spacing);
            
            bool can_install = !GUI::IsAppletMode();
            if (!can_install) {
                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
            }
            
            if (ImGui::Button(strings[lang][Lang::ButtonInstall], ImVec2(button_width, 35))) {
                ImGui::CloseCurrentPopup();
                installing = true;
            }
            
            if (can_install) {
                ImGui::SetItemDefaultFocus();
            }
            
            if (!can_install) {
                ImGui::PopItemFlag();
                ImGui::PopStyleVar();
            }
            
            ImGui::EndPopup();
        }
    }
}
