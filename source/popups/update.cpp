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
    static std::string update_path;

    void UpdatePopup(App &app, bool &state, bool &connection_status, bool &available, const std::string &tag) {
        const int lang = app.config.Lang();
        Popups::SetupPopup(app, strings[lang][Lang::UpdateTitle]);
        
        // Handle no connection case immediately with toast - don't show popup
        if (!connection_status) {
            Toast::Show(strings[lang][Lang::UpdateNetworkError], false, 3.0f);
            state = false;
            return;
        }
        
        // Handle no update available case with toast - don't show popup
        if (!available || tag.empty()) {
            Toast::Show(strings[lang][Lang::UpdateNotAvailable], true, 3.0f);
            state = false;
            return;
        }
        
        if (ImGui::BeginPopupModal(strings[lang][Lang::UpdateTitle], nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar)) {
            // Update available, waiting for user to download
            ImGui::Text(strings[lang][Lang::UpdateAvailable]);
            std::string text = strings[lang][Lang::UpdatePrompt] + tag + "?";
            ImGui::Text(text.c_str());

            ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
            
            // OK to download, Cancel to close
            if (ImGui::Button(strings[lang][Lang::ButtonOK], ImVec2(120, 36))) {
                update_path = Net::GetLatestReleaseNRO(app.fs, tag);
                if (update_path.empty()) {
                    // Download failed - show toast and close popup
                    Toast::Show("Download failed", false, 3.0f);
                    ImGui::CloseCurrentPopup();
                    state = false;
                    update_path.clear();
                } else {
                    // Download successful - restart immediately
                    std::string full_path = "sdmc:" + update_path;
                    
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
                    
                    envSetNextLoad(full_path.c_str(), full_path.c_str());
                    Log::Debug("envSetNextLoad set to: %s\n", full_path.c_str());
                    
                    ImGui::CloseCurrentPopup();
                    state = false;
                    update_path.clear();
                    app.request_exit = true;
                }
            }
            ImGui::SetItemDefaultFocus();
            
            ImGui::SameLine(0.0f, 15.0f);
            
            if (ImGui::Button(strings[lang][Lang::ButtonCancel], ImVec2(120, 36))) {
                ImGui::CloseCurrentPopup();
                state = false;
                update_path.clear();
            }
        }
        
        Popups::ExitPopup();
    }
}
