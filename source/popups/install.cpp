#include "config.hpp"
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
    static bool dont_show_again = false;  // Local checkbox state
}

namespace Popups {
    void SetInstallPath(const std::string &path, bool is_nsp) {
        install_path = path;
        is_nsp_file = is_nsp;
        installing = false;
        dont_show_again = false;  // Reset checkbox for new install
        
        // Extract filename
        size_t pos = path.find_last_of("/\\");
        install_filename = (pos != std::string::npos) ? path.substr(pos + 1) : path;
    }
    
    void InstallPopup(App &app) {
        const int lang = app.config.Lang();
        
        // If installing, show progress
        if (installing) {
            Popups::SetupPopup(app, strings[lang][Lang::NROForwarderBuilding]);
            
            if (ImGui::BeginPopupModal(strings[lang][Lang::NROForwarderBuilding], nullptr, 
                    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar)) {
                ImGui::TextUnformatted(strings[lang][Lang::NROForwarderBuilding]);
                ImGui::Dummy(ImVec2(0.0f, 10.0f));
                
                static float progress_anim = 0.0f;
                progress_anim += ImGui::GetIO().DeltaTime * 0.5f;
                if (progress_anim > 1.0f) progress_anim -= 1.0f;
                ImGui::ProgressBar(progress_anim, ImVec2(350.0f, 0.0f), "");
            }
            
            Popups::ExitPopup();
            
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
        
        bool show_warning = !app.config.HideInstallWarning();
        bool is_applet = GUI::IsAppletMode();
        
        // Skip dialog and install directly if warning is hidden and not in applet mode
        if (!show_warning && !is_applet) {
            installing = true;
            return;
        }
        
        // Confirmation popup
        Popups::SetupPopup(app, strings[lang][Lang::ButtonInstall]);
        
        if (ImGui::BeginPopupModal(strings[lang][Lang::ButtonInstall], nullptr, 
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar)) {
            
            // Warning message (if not hidden)
            if (show_warning) {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), 
                    "%s", strings[lang][Lang::InstallWarningMessage]);
                ImGui::Dummy(ImVec2(0.0f, 5.0f));
                
                // "Don't show again" checkbox (local state, saved on Install)
                ImGui::Checkbox(strings[lang][Lang::InstallDontShowAgain], &dont_show_again);
                ImGui::Dummy(ImVec2(0.0f, 5.0f));
            }
            
            // Applet mode warning
            if (is_applet) {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), 
                    "%s", strings[lang][Lang::NSPInstallNotAvailable]);
                ImGui::Dummy(ImVec2(0.0f, 5.0f));
            }
            
            // Buttons - Install first (pre-highlighted), Cancel second
            float button_width = 120.0f;
            float spacing = 15.0f;
            
            bool can_install = !is_applet;
            if (!can_install) {
                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
            }
            
            if (ImGui::Button(strings[lang][Lang::ButtonInstall], ImVec2(button_width, 36))) {
                // Save "don't show again" preference if checked
                if (dont_show_again) {
                    app.config.normal.hide_install_warning = true;
                    Config::Save(app.config, app.fs);
                }
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
            
            ImGui::SameLine(0, spacing);
            
            if (ImGui::Button(strings[lang][Lang::ButtonCancel], ImVec2(button_width, 36))) {
                install_path.clear();
                install_filename.clear();
                ImGui::CloseCurrentPopup();
                app.window.state = WINDOW_STATE_FILEBROWSER;
            }
        }
        
        Popups::ExitPopup();
    }
}
