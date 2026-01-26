#include <cstring>

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

    void ReplacePopup(App &app, bool is_move) {
        WindowData &data = app.window;
        const int lang = app.config.Lang();
        Popups::SetupPopup(app, strings[lang][Lang::ReplaceTitle]);
        
        if (ImGui::BeginPopupModal(strings[lang][Lang::ReplaceTitle], nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("%s", strings[lang][Lang::ReplaceMessage]);
            
            ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
            
            if (ImGui::Button(strings[lang][Lang::ReplaceButton], ImVec2(120, 0))) {
                // Build destination path and delete existing file/directory first
                std::string dest_path = FS::BuildPath(app.fs, FS::GetCopyEntryFilename(app.fs), true);
                if (!FS::DeletePath(dest_path)) {
                    Log::Error("ReplacePopup: Failed to delete existing destination: %s\n", dest_path.c_str());
                    // Continue anyway - the copy/move might still succeed
                }
                
                bool ret = false;
                if (is_move) {
                    ret = FS::Move(app);
                    move = false;
                } else {
                    ImGui::EndPopup();
                    ImGui::PopStyleVar();
                    ImGui::Render();
                    
                    ret = FS::Paste(app);
                    copy = false;
                    
                    if (ret) {
                        if (!FS::RefreshDirectory(app.fs, app.selection, data.entries, data.metadata_cache, true)) {
                            Log::Error("ReplacePopup: Failed to refresh directory after paste\n");
                        }
                        data.sort = -1;
                    }
                    
                    data.state = WINDOW_STATE_FILEBROWSER;
                    return;
                }
                
                if (ret) {
                    if (!FS::RefreshDirectory(app.fs, app.selection, data.entries, data.metadata_cache, true)) {
                        Log::Error("ReplacePopup: Failed to refresh directory after move\n");
                    }
                    data.sort = -1;
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

    void MultiReplacePopup(App &app, bool is_move, size_t conflict_count, size_t total_count) {
        WindowData &data = app.window;
        const int lang = app.config.Lang();
        Popups::SetupPopup(app, strings[lang][Lang::ReplaceTitle]);
        
        if (ImGui::BeginPopupModal(strings[lang][Lang::ReplaceTitle], nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            // Format the message with conflict count
            char msg_buffer[256];
            std::snprintf(msg_buffer, sizeof(msg_buffer), strings[lang][Lang::MultiReplaceMessage], 
                         conflict_count, total_count);
            ImGui::Text("%s", msg_buffer);
            
            ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
            
            // Replace All button
            if (ImGui::Button(strings[lang][Lang::MultiReplaceAll], ImVec2(150, 0))) {
                FS::SetConflictHandling(app.fs, ConflictHandling_ReplaceAll);
                ImGui::CloseCurrentPopup();
                data.state = WINDOW_STATE_OPTIONS;  // Return to options to execute copy/move
            }
            
            ImGui::SameLine(0.0f, 15.0f);
            
            // Skip Existing button
            if (ImGui::Button(strings[lang][Lang::MultiReplaceSkip], ImVec2(150, 0))) {
                FS::SetConflictHandling(app.fs, ConflictHandling_SkipAll);
                ImGui::CloseCurrentPopup();
                data.state = WINDOW_STATE_OPTIONS;  // Return to options to execute copy/move
            }
            
            ImGui::SameLine(0.0f, 15.0f);
            
            // Cancel button
            if (ImGui::Button(strings[lang][Lang::ButtonCancel], ImVec2(120, 0))) {
                Popups::ClearPendingMultiOperation(app.fs);
                copy = false;
                move = false;
                ImGui::CloseCurrentPopup();
                data.state = WINDOW_STATE_OPTIONS;
            }
        }
        
        Popups::ExitPopup();
    }
}
