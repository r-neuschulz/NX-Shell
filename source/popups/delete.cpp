#include <cstring>

#include "config.hpp"
#include "fs.hpp"
#include "gui.hpp"
#include "imgui.h"
#include "language.hpp"
#include "log.hpp"
#include "popups.hpp"
#include "selection.hpp"

namespace Popups {
    void DeletePopup(App &app) {
        SelectionStore selection(app.selection);
        WindowData &data = app.window;
        const int lang = app.config.Lang();
        Popups::SetupPopup(app, strings[lang][Lang::OptionsDelete]);
        
        if (ImGui::BeginPopupModal(strings[lang][Lang::OptionsDelete], nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text(strings[lang][Lang::DeleteMessage]);
            
            // Check if we have multiple selections
            if (selection.Count() > 1) {
                ImGui::Text(strings[lang][Lang::DeleteMultiplePrompt]);
                ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
                ImGui::BeginChild("Scrolling", ImVec2(0, 100));
                for (const auto &path : selection.GetSelectedPaths()) {
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
            
            if (ImGui::Button(strings[lang][Lang::ButtonOK], ImVec2(120, 36))) {
                bool ret = false;

                if (selection.Count() > 1) {
                    Log::Exit();

                    // Delete all selected items using full paths directly
                    for (const auto &full_path : selection.GetSelectedPaths()) {
                        std::string filename = SelectionStore::GetFilename(full_path);
                        if (filename == "..")
                            continue;
                        
                        ret = FS::DeletePath(full_path);
                        
                        if (!ret) {
                            if (!FS::RefreshDirectory(app.fs, app.selection, data.entries, data.metadata_cache, true)) {
                                Log::Error("DeletePopup: Failed to refresh directory after partial multi-delete\n");
                            }
                            break;
                        }
                    }
                    
                    Log::Init(app);
                }
                else {
                    if ((std::strncmp(data.entries[data.selected].name, "..", 2)) != 0)
                        ret = FS::Delete(app.fs, data.entries[data.selected]);
                }
                
                if (ret) {
                    if (!FS::RefreshDirectory(app.fs, app.selection, data.entries, data.metadata_cache, true)) {
                        Log::Error("DeletePopup: Failed to refresh directory after delete\n");
                    }
                    Toast::Show(strings[lang][Lang::DeleteSuccess], true, 3.0f);
                }

                data.sort = -1;
                ImGui::CloseCurrentPopup();
                data.state = WINDOW_STATE_FILEBROWSER;
            }
            ImGui::SetItemDefaultFocus();
            
            ImGui::SameLine(0.0f, 15.0f);
            
            if (ImGui::Button(strings[lang][Lang::ButtonCancel], ImVec2(120, 36))) {
                ImGui::CloseCurrentPopup();
                data.state = WINDOW_STATE_OPTIONS;
            }
        }
        
        Popups::ExitPopup();
    }
}
