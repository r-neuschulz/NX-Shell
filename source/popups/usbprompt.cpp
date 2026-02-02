#include "config.hpp"
#include "fs.hpp"
#include "imgui.h"
#include "services.hpp"
#include "language.hpp"
#include "log.hpp"
#include "popups.hpp"
#include "selection.hpp"
#include "usb.hpp"
#include "windows.hpp"

namespace Popups {
    static bool done = false;

    void USBPopup(App &app, bool &state) {
        const int lang = app.config.Lang();
        Popups::SetupPopup(app, strings[lang][Lang::SettingsUSBTitle]);

        if (ImGui::BeginPopupModal(strings[lang][Lang::SettingsUSBTitle], nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar)) {
            if (!done)
                ImGui::Text(strings[lang][Lang::USBUnmountPrompt]);
            else
                ImGui::Text(strings[lang][Lang::USBUnmountSuccess]);
            
            ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
            
            if (ImGui::Button(strings[lang][Lang::ButtonOK], ImVec2(120, 36))) {
                if (!done) {
                    USB::Unmount();
                    
                    // Reset device back to sdmc
                    app.fs.device = "sdmc:";
                    app.fs.current_fs = std::addressof(app.fs.devices[FileSystemSDMC]);
                    
                    app.fs.cwd = "/";
                    app.window.entries.clear();
                    if (!FS::RefreshDirectory(app.fs, app.selection, app.window.entries, app.window.metadata_cache, true)) {
                        Log::Error("USBPopup: Failed to refresh directory after USB unmount\n");
                    }
                    FS::GetUsedStorageSpace(app.fs, app.window.used_storage);
                    FS::GetTotalStorageSpace(app.fs, app.window.total_storage);
                    app.window.sort = -1;
                    
                    ImGui::CloseCurrentPopup();
                    done = true;
                }
                else {
                    ImGui::CloseCurrentPopup();
                    done = false;
                    state = false;
                }
            }
            ImGui::SetItemDefaultFocus();
            
            ImGui::SameLine(0.0f, 15.0f);
            
            if (!done) {
                if (ImGui::Button(strings[lang][Lang::ButtonCancel], ImVec2(120, 36))) {
                    ImGui::CloseCurrentPopup();
                    state = false;
                }
            }
        }

        Popups::ExitPopup();
    }
}
