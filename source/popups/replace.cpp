#include <cstdio>
#include <cstring>
#include <sys/stat.h>

#include "config.hpp"
#include "fs.hpp"
#include "imgui.h"
#include "language.hpp"
#include "log.hpp"
#include "popups.hpp"
#include "selection.hpp"

namespace Popups {
    // External state from options.cpp
    extern bool copy;
    extern bool move;
    extern bool pending_replace_is_move;

    void ReplacePopup(WindowData &data, bool is_move) {
        const int lang = Config::GetLang();
        Popups::SetupPopup(strings[lang][Lang::ReplaceTitle]);
        
        if (ImGui::BeginPopupModal(strings[lang][Lang::ReplaceTitle], nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("%s", strings[lang][Lang::ReplaceMessage]);
            
            ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
            
            if (ImGui::Button(strings[lang][Lang::ReplaceButton], ImVec2(120, 0))) {
                // Build destination path and delete existing file/directory first
                std::string dest_path = FS::BuildPath(FS::GetCopyEntryFilename(), true);
                
                struct stat dest_stat = { 0 };
                if (stat(dest_path.c_str(), std::addressof(dest_stat)) == 0) {
                    // Delete the existing destination
                    if (S_ISDIR(dest_stat.st_mode)) {
                        FS::DeleteRecursive(dest_path);
                    } else {
                        remove(dest_path.c_str());
                    }
                }
                
                bool ret = false;
                if (is_move) {
                    ret = FS::Move();
                    move = false;
                } else {
                    ImGui::EndPopup();
                    ImGui::PopStyleVar();
                    ImGui::Render();
                    
                    ret = FS::Paste();
                    copy = false;
                    
                    if (ret) {
                        FS::GetDirList(device, cwd, data.entries);
                        FS::PopulateMetadataCache(data.entries, data.metadata_cache);
                        g_selection.Clear();
                        sort = -1;
                    }
                    
                    data.state = WINDOW_STATE_FILEBROWSER;
                    return;
                }
                
                if (ret) {
                    FS::GetDirList(device, cwd, data.entries);
                    FS::PopulateMetadataCache(data.entries, data.metadata_cache);
                    g_selection.Clear();
                    sort = -1;
                }
                
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

    void MultiReplacePopup(WindowData &data, bool is_move, size_t conflict_count, size_t total_count) {
        const int lang = Config::GetLang();
        Popups::SetupPopup(strings[lang][Lang::ReplaceTitle]);
        
        if (ImGui::BeginPopupModal(strings[lang][Lang::ReplaceTitle], nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            // Format the message with conflict count
            char msg_buffer[256];
            std::snprintf(msg_buffer, sizeof(msg_buffer), strings[lang][Lang::MultiReplaceMessage], 
                         conflict_count, total_count);
            ImGui::Text("%s", msg_buffer);
            
            ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
            
            // Replace All button
            if (ImGui::Button(strings[lang][Lang::MultiReplaceAll], ImVec2(150, 0))) {
                FS::SetConflictHandling(ConflictHandling_ReplaceAll);
                ImGui::CloseCurrentPopup();
                data.state = WINDOW_STATE_OPTIONS;  // Return to options to execute copy/move
            }
            
            ImGui::SameLine(0.0f, 15.0f);
            
            // Skip Existing button
            if (ImGui::Button(strings[lang][Lang::MultiReplaceSkip], ImVec2(150, 0))) {
                FS::SetConflictHandling(ConflictHandling_SkipAll);
                ImGui::CloseCurrentPopup();
                data.state = WINDOW_STATE_OPTIONS;  // Return to options to execute copy/move
            }
            
            ImGui::SameLine(0.0f, 15.0f);
            
            // Cancel button
            if (ImGui::Button(strings[lang][Lang::ButtonCancel], ImVec2(120, 0))) {
                Popups::ClearPendingMultiOperation();
                copy = false;
                move = false;
                ImGui::CloseCurrentPopup();
                data.state = WINDOW_STATE_OPTIONS;
            }
        }
        
        Popups::ExitPopup();
    }
}
