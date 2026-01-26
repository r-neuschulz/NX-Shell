#include "config.hpp"
#include "fs.hpp"
#include "imgui.h"
#include "language.hpp"
#include "log.hpp"
#include "popups.hpp"
#include "selection.hpp"
#include "usb.hpp"
#include "windows.hpp"

namespace Popups {
    static bool done = false;

    void USBPopup(bool &state) {
        const int lang = Config::GetLang();
        Popups::SetupPopup(strings[lang][Lang::SettingsUSBTitle]);

        if (ImGui::BeginPopupModal(strings[lang][Lang::SettingsUSBTitle], nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            if (!done)
                ImGui::Text(strings[lang][Lang::USBUnmountPrompt]);
            else
                ImGui::Text(strings[lang][Lang::USBUnmountSuccess]);
            
            ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
            
            if (ImGui::Button(strings[lang][Lang::ButtonOK], ImVec2(120, 0))) {
                if (!done) {
                    USB::Unmount();
                    
                    // Reset device back to sdmc
                    device = "sdmc:";
                    fs = std::addressof(devices[FileSystemSDMC]);
                    
                    cwd = "/";
                    data.entries.clear();
                    if (!FS::RefreshDirectory(data.entries, data.metadata_cache, true)) {
                        Log::Error("USBPopup: Failed to refresh directory after USB unmount\n");
                    }
                    FS::GetUsedStorageSpace(data.used_storage);
                    FS::GetTotalStorageSpace(data.total_storage);
                    sort = -1;
                    
                    ImGui::CloseCurrentPopup();
                    done = true;
                }
                else {
                    ImGui::CloseCurrentPopup();
                    done = false;
                    state = false;
                }
            }
            
            ImGui::SameLine(0.0f, 15.0f);
            
            if (!done) {
                if (ImGui::Button(strings[lang][Lang::ButtonCancel], ImVec2(120, 0))) {
                    ImGui::CloseCurrentPopup();
                    state = false;
                }
            }
        }

        Popups::ExitPopup();
    }
}
