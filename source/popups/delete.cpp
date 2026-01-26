#include <cstring>

#include "config.hpp"
#include "fs.hpp"
#include "imgui.h"
#include "language.hpp"
#include "log.hpp"
#include "popups.hpp"
#include "selection.hpp"

namespace Popups {
    void DeletePopup(WindowData &data) {
        const int lang = Config::GetLang();
        Popups::SetupPopup(strings[lang][Lang::OptionsDelete]);
        
        if (ImGui::BeginPopupModal(strings[lang][Lang::OptionsDelete], nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text(strings[lang][Lang::DeleteMessage]);
            
            // Check if we have multiple selections
            if (g_selection.Count() > 1) {
                ImGui::Text(strings[lang][Lang::DeleteMultiplePrompt]);
                ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
                ImGui::BeginChild("Scrolling", ImVec2(0, 100));
                for (const auto &path : g_selection.GetSelectedPaths()) {
                    std::string filename = SelectionStore::GetFilename(path);
                    ImGui::Text("%s", filename.c_str());
                }
                ImGui::EndChild();
            }
            else {
                std::string text = strings[lang][Lang::DeletePrompt] + std::string(data.entries[data.selected].name) + "?";
                ImGui::Text("%s", text.c_str());
            }
            
            ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
            
            if (ImGui::Button(strings[lang][Lang::ButtonOK], ImVec2(120, 0))) {
                bool ret = false;

                if (g_selection.Count() > 1) {
                    Log::Exit();

                    // Delete all selected items using full paths directly
                    for (const auto &full_path : g_selection.GetSelectedPaths()) {
                        std::string filename = SelectionStore::GetFilename(full_path);
                        if (filename == "..")
                            continue;
                        
                        ret = FS::DeletePath(full_path);
                        
                        if (!ret) {
                            if (!FS::RefreshDirectory(data.entries, data.metadata_cache, true)) {
                                Log::Error("DeletePopup: Failed to refresh directory after partial multi-delete\n");
                            }
                            break;
                        }
                    }
                    
                    Log::Init();
                }
                else {
                    if ((std::strncmp(data.entries[data.selected].name, "..", 2)) != 0)
                        ret = FS::Delete(data.entries[data.selected]);
                }
                
                if (ret) {
                    if (!FS::RefreshDirectory(data.entries, data.metadata_cache, true)) {
                        Log::Error("DeletePopup: Failed to refresh directory after delete\n");
                    }
                }

                sort = -1;
                ImGui::CloseCurrentPopup();
                data.state = WINDOW_STATE_FILEBROWSER;
            }
            
            ImGui::SameLine(0.0f, 15.0f);
            
            if (ImGui::Button(strings[lang][Lang::ButtonCancel], ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
                data.state = WINDOW_STATE_OPTIONS;
            }
        }
        
        Popups::ExitPopup();
    }
}
