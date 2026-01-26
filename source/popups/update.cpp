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

    void UpdatePopup(App &app, bool &state, bool &connection_status, bool &available, const std::string &tag) {
        const int lang = app.config.Lang();
        Popups::SetupPopup(app, strings[lang][Lang::UpdateTitle]);
        
        if (ImGui::BeginPopupModal(strings[lang][Lang::UpdateTitle], nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            if (!connection_status)
                ImGui::Text(strings[lang][Lang::UpdateNetworkError]);
            else if ((connection_status) && (available) && (!tag.empty()) && (!done)) {
                ImGui::Text(strings[lang][Lang::UpdateAvailable]);
                std::string text = strings[lang][Lang::UpdatePrompt] + tag + "?";
                ImGui::Text(text.c_str());
            }
            else if (done) {
                ImGui::Text(strings[lang][Lang::UpdateSuccess]);
                ImGui::Text(strings[lang][Lang::UpdateRestart]);
            }
            else
                ImGui::Text(strings[lang][Lang::UpdateNotAvailable]);

            ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
            
            if (ImGui::Button(strings[lang][Lang::ButtonOK], ImVec2(120, 0))) {
                if ((connection_status) && (available) && (!tag.empty()) && (!done)) {
                    Net::GetLatestReleaseNRO(app.fs, tag);
                    
                    Result ret = 0;
                    if (R_FAILED(ret = fsFsDeleteFile(std::addressof(app.fs.devices[FileSystemSDMC]), __application_path)))
                        Log::Error("fsFsDeleteFile(%s) failed: 0x%x\n", __application_path, ret);
                    
                    if (R_FAILED(ret = fsFsRenameFile(std::addressof(app.fs.devices[FileSystemSDMC]), "/switch/NX-Shell/NX-Shell_UPDATE.nro", __application_path)))
                        Log::Error("fsFsRenameFile(update) failed: 0x%x\n", ret);
                        
                    done = true;
                }
                else {
                    ImGui::CloseCurrentPopup();
                    state = false;
                }
            }

            if ((connection_status) && (available) && (!done)) {
                ImGui::SameLine(0.0f, 15.0f);
                
                if (ImGui::Button(strings[lang][Lang::ButtonCancel], ImVec2(120, 0))) {
                    ImGui::CloseCurrentPopup();
                    state = false;
                }
            }
        }
        
        Popups::ExitPopup();
    }
}
