#include <switch.h>

#include "config.hpp"
#include "fs.hpp"
#include "gui.hpp"
#include "imgui.h"
#include "language.hpp"
#include "log.hpp"
#include "net.hpp"
#include "popups.hpp"
#include "services.hpp"
#include "utils.hpp"
#include "windows.hpp"

namespace Popups {
    static bool done = false;
    static bool download_failed = false;
    static std::string update_path;

    void UpdatePopup(App &app, bool &state, bool &connection_status, bool &available, const std::string &tag) {
        const int lang = app.config.Lang();
        Popups::SetupPopup(app, strings[lang][Lang::UpdateTitle]);
        
        if (ImGui::BeginPopupModal(strings[lang][Lang::UpdateTitle], nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            // Display message based on state
            if (!connection_status) {
                ImGui::Text(strings[lang][Lang::UpdateNetworkError]);
            }
            else if (download_failed) {
                // Download was attempted but failed (e.g., 404, network error during download)
                ImGui::Text(strings[lang][Lang::UpdateNetworkError]);
                ImGui::TextWrapped("Download failed. The update file could not be retrieved.");
            }
            else if (done && !update_path.empty()) {
                // Download successful
                ImGui::Text(strings[lang][Lang::UpdateSuccess]);
                ImGui::Text(strings[lang][Lang::UpdateRestart]);
            }
            else if (connection_status && available && !tag.empty()) {
                // Update available, waiting for user to download
                ImGui::Text(strings[lang][Lang::UpdateAvailable]);
                std::string text = strings[lang][Lang::UpdatePrompt] + tag + "?";
                ImGui::Text(text.c_str());
            }
            else {
                ImGui::Text(strings[lang][Lang::UpdateNotAvailable]);
            }

            ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
            
            if (done && !update_path.empty()) {
                // After successful download: Restart button launches new version
                if (ImGui::Button(strings[lang][Lang::ButtonRestart], ImVec2(120, 36))) {
                    // Tell homebrew loader to load the update NRO on exit
                    std::string full_path = "sdmc:" + update_path;
                    
                    // #region agent log - Verify file before envSetNextLoad
                    Log::Debug("[DBGMODE] Pre-envSetNextLoad: update_path='%s', full_path='%s'\n", 
                               update_path.c_str(), full_path.c_str());
                    s64 final_size = 0;
                    FsFile check_file;
                    if (R_SUCCEEDED(fsFsOpenFile(std::addressof(app.fs.devices[FileSystemSDMC]), 
                                    update_path.c_str(), FsOpenMode_Read, std::addressof(check_file)))) {
                        fsFileGetSize(&check_file, &final_size);
                        fsFileClose(&check_file);
                        Log::Debug("[DBGMODE] NRO file verified: size=%lld bytes, exists=true\n", (long long)final_size);
                    } else {
                        Log::Debug("[DBGMODE] NRO file verification FAILED: cannot open '%s'\n", update_path.c_str());
                    }
                    Log::Flush();
                    // #endregion
                    
                    envSetNextLoad(full_path.c_str(), full_path.c_str());
                    Log::Debug("envSetNextLoad set to: %s\n", full_path.c_str());
                    
                    ImGui::CloseCurrentPopup();
                    state = false;
                    done = false;
                    download_failed = false;
                    update_path.clear();
                    app.request_exit = true;
                }
                ImGui::SetItemDefaultFocus();
                
                ImGui::SameLine(0.0f, 15.0f);
                
                if (ImGui::Button(strings[lang][Lang::ButtonCancel], ImVec2(120, 36))) {
                    ImGui::CloseCurrentPopup();
                    state = false;
                    done = false;
                    download_failed = false;
                    update_path.clear();
                }
            }
            else if (download_failed || !connection_status) {
                // Download failed or no connection: just show OK to close
                if (ImGui::Button(strings[lang][Lang::ButtonOK], ImVec2(120, 36))) {
                    ImGui::CloseCurrentPopup();
                    state = false;
                    done = false;
                    download_failed = false;
                    update_path.clear();
                }
                ImGui::SetItemDefaultFocus();
            }
            else if (connection_status && available && !tag.empty()) {
                // Update available: OK to download, Cancel to close
                if (ImGui::Button(strings[lang][Lang::ButtonOK], ImVec2(120, 36))) {
                    update_path = Net::GetLatestReleaseNRO(app.fs, tag);
                    if (update_path.empty()) {
                        download_failed = true;
                    } else {
                        done = true;
                    }
                }
                ImGui::SetItemDefaultFocus();
                
                ImGui::SameLine(0.0f, 15.0f);
                
                if (ImGui::Button(strings[lang][Lang::ButtonCancel], ImVec2(120, 36))) {
                    ImGui::CloseCurrentPopup();
                    state = false;
                    done = false;
                    download_failed = false;
                    update_path.clear();
                }
            }
            else {
                // No update available: just show OK to close
                if (ImGui::Button(strings[lang][Lang::ButtonOK], ImVec2(120, 36))) {
                    ImGui::CloseCurrentPopup();
                    state = false;
                }
                ImGui::SetItemDefaultFocus();
            }
        }
        
        Popups::ExitPopup();
    }
    
    void UpdateWelcomePopup(App &app, bool &state, const std::string &version) {
        Popups::SetupPopup(app, "Welcome");
        
        if (ImGui::BeginPopupModal("Welcome", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            // Show welcome message with new version
            std::string welcome_text = "Welcome to NX-Shell " + version + "!";
            ImGui::Text("%s", welcome_text.c_str());
            
            ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
            
            const int lang = app.config.Lang();
            if (ImGui::Button(strings[lang][Lang::ButtonOK], ImVec2(120, 36))) {
                ImGui::CloseCurrentPopup();
                state = false;
            }
            ImGui::SetItemDefaultFocus();
        }
        
        Popups::ExitPopup();
    }
}
