#include <cstring>
#include <sys/stat.h>

#include "config.hpp"
#include "fs.hpp"
#include "imgui_internal.h"
#include "keyboard.hpp"
#include "language.hpp"
#include "popups.hpp"
#include "selection.hpp"

namespace Options {
    // Track pending multi-file operation after user chooses conflict handling
    static bool pending_multi_copy = false;
    static bool pending_multi_move = false;
    
    static void RefreshEntries(bool clear_selection) {
        FS::RefreshDirectory(data.entries, data.metadata_cache, clear_selection);
    }

    static void HandleMultipleCopy(WindowData &data, bool (*func)(), bool is_move) {
        // Process all selected paths from SelectionStore
        const auto &selected_paths = g_selection.GetSelectedPaths();
        ConflictHandling conflict_mode = FS::GetConflictHandling();
        
        for (const auto &full_path : selected_paths) {
            // Extract filename from path
            std::string filename = SelectionStore::GetFilename(full_path);
            if (filename == "..")
                continue;
            
            // Build destination path to check for conflicts
            std::string dest_path = FS::BuildPath(filename, true);
            
            // Handle conflict based on user's choice
            if (conflict_mode == ConflictHandling_SkipAll) {
                // Skip if destination exists
                if (FS::ShouldSkipDueToConflict(dest_path))
                    continue;
            }
            else if (conflict_mode == ConflictHandling_ReplaceAll) {
                // Delete existing destination first if it exists
                FS::DeletePath(dest_path);
            }
            
            // Extract parent directory path
            std::string parent_path = SelectionStore::GetParentPath(full_path);
            
            // Create entry for this item
            FsDirectoryEntry entry;
            std::strncpy(entry.name, filename.c_str(), sizeof(entry.name) - 1);
            entry.name[sizeof(entry.name) - 1] = '\0';
            
            // Determine if it's a file or directory
            std::string stat_path = full_path;
            struct stat path_stat;
            if (stat(stat_path.c_str(), &path_stat) == 0) {
                entry.type = S_ISDIR(path_stat.st_mode) ? FsDirEntryType_Dir : FsDirEntryType_File;
                entry.file_size = path_stat.st_size;
            } else {
                entry.type = FsDirEntryType_File;  // Default to file if stat fails
                entry.file_size = 0;
            }
            
            FS::Copy(entry, parent_path);

            if (!(*func)()) {
                Options::RefreshEntries(true);
                break;
            }
        }
        
        // Clear conflict handling mode after operation completes
        FS::ClearConflictHandling();
        
        Options::RefreshEntries(true);
        sort = -1;
    }
}

namespace Popups {
    bool copy = false, move = false;
    bool pending_replace_is_move = false;
    static bool show_recursive_error = false;
    
    // Multi-file conflict tracking
    static size_t multi_conflict_count = 0;
    static size_t multi_total_count = 0;

    void OptionsPopup(WindowData &data) {
        const int lang = Config::GetLang();
        
        // Check if we're returning from multi-replace popup with a chosen action
        if (FS::GetConflictHandling() != ConflictHandling_Ask) {
            if (Options::pending_multi_copy) {
                Options::pending_multi_copy = false;
                Options::HandleMultipleCopy(data, std::addressof(FS::Paste), false);
                copy = false;
                data.state = WINDOW_STATE_FILEBROWSER;
                return;
            }
            else if (Options::pending_multi_move) {
                Options::pending_multi_move = false;
                Options::HandleMultipleCopy(data, std::addressof(FS::Move), true);
                move = false;
                data.state = WINDOW_STATE_FILEBROWSER;
                return;
            }
        }
        
        Popups::SetupPopup(strings[lang][Lang::OptionsTitle]);

        if (ImGui::BeginPopupModal(strings[lang][Lang::OptionsTitle], nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            if (ImGui::Button(strings[lang][Lang::OptionsSelectAll], ImVec2(200, 50))) {
                // Clear any previous selections and select all in current directory
                g_selection.Clear();
                std::string base_path = device + cwd;
                g_selection.SelectAll(base_path, data.entries);
            }

            ImGui::SameLine(0.0f, 15.0f);
            
            if (ImGui::Button(strings[lang][Lang::OptionsClearAll], ImVec2(200, 50))) {
                g_selection.Clear();
                copy = false;
                move = false;
            }

            ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing

            if (ImGui::Button(strings[lang][Lang::OptionsProperties], ImVec2(200, 50))) {
                ImGui::CloseCurrentPopup();
                data.state = WINDOW_STATE_PROPERTIES;
            }

            ImGui::SameLine(0.0f, 15.0f);

            if (ImGui::Button(strings[lang][Lang::OptionsRename], ImVec2(200, 50))) {
                std::string path = Keyboard::GetText(strings[lang][Lang::OptionsRenamePrompt], data.entries[data.selected].name);
                
                if (FS::Rename(data.entries[data.selected], path.c_str())) {
                    Options::RefreshEntries(false);
                    sort = -1;
                }
                
                ImGui::CloseCurrentPopup();
                data.state = WINDOW_STATE_FILEBROWSER;
            }
            
            ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
            
            if (ImGui::Button(strings[lang][Lang::OptionsNewFolder], ImVec2(200, 50))) {
                std::string name = Keyboard::GetText(strings[lang][Lang::OptionsFolderPrompt], strings[lang][Lang::OptionsNewFolder]);
                std::string path = FS::BuildPath(name, true);

                if (R_SUCCEEDED(mkdir(path.c_str(), 0700))) {
                    Options::RefreshEntries(true);
                    sort = -1;
                }
                
                ImGui::CloseCurrentPopup();
                data.state = WINDOW_STATE_FILEBROWSER;
            }
            
            ImGui::SameLine(0.0f, 15.0f);
            
            if (ImGui::Button(strings[lang][Lang::OptionsNewFile], ImVec2(200, 50))) {
                std::string name = Keyboard::GetText(strings[lang][Lang::OptionsFilePrompt], strings[lang][Lang::OptionsNewFile]);
                std::string path = FS::BuildPath(name, true);
                
                FILE *file = fopen(path.c_str(), "w");
                fclose(file);
                
                if (FS::FileExists(path)) {
                    Options::RefreshEntries(true);
                    sort = -1;
                }
                
                ImGui::CloseCurrentPopup();
                data.state = WINDOW_STATE_FILEBROWSER;
            }
            
            ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
            
            if (ImGui::Button(!copy? strings[lang][Lang::OptionsCopy] : strings[lang][Lang::OptionsPaste], ImVec2(200, 50))) {
                if (!copy) {
                    // If no selections, copy the currently focused item
                    if (g_selection.Count() == 0) {
                        std::string path = device + cwd;
                        FS::Copy(data.entries[data.selected], path);
                    }
                        
                    copy = !copy;
                    data.state = WINDOW_STATE_FILEBROWSER;
                }
                else {
                    // Check for recursive copy before attempting paste
                    if (FS::WouldCauseRecursiveCopy()) {
                        show_recursive_error = true;
                    }
                    // Check if destination already exists (single file only)
                    else if (g_selection.Count() <= 1 && FS::DestinationExists()) {
                        pending_replace_is_move = false;
                        ImGui::CloseCurrentPopup();
                        data.state = WINDOW_STATE_REPLACE;
                    }
                    // Multiple files - check for conflicts
                    else if (g_selection.Count() > 1) {
                        multi_conflict_count = FS::CountConflicts();
                        if (multi_conflict_count > 0) {
                            // Has conflicts - show multi-replace popup
                            multi_total_count = g_selection.Count();
                            Options::pending_multi_copy = true;
                            ImGui::CloseCurrentPopup();
                            data.state = WINDOW_STATE_MULTI_REPLACE;
                        }
                        else {
                            // No conflicts - proceed with copy
                            ImGui::EndPopup();
                            ImGui::PopStyleVar();
                            ImGui::Render();
                            
                            Options::HandleMultipleCopy(data, std::addressof(FS::Paste), false);
                            copy = !copy;
                            data.state = WINDOW_STATE_FILEBROWSER;
                            return;
                        }
                    }
                    else {
                        ImGui::EndPopup();
                        ImGui::PopStyleVar();
                        ImGui::Render();

                        if (FS::Paste()) {
                            Options::RefreshEntries(true);
                            sort = -1;
                        }

                        copy = !copy;
                        data.state = WINDOW_STATE_FILEBROWSER;
                        return;
                    }
                }
            }
            
            ImGui::SameLine(0.0f, 15.0f);
            
            if (ImGui::Button(!move? strings[lang][Lang::OptionsMove] : strings[lang][Lang::OptionsPaste], ImVec2(200, 50))) {
                if (!move) {
                    // If no selections, copy the currently focused item
                    if (g_selection.Count() == 0) {
                        std::string path = device + cwd;
                        FS::Copy(data.entries[data.selected], path);
                    }
                    
                    move = !move;
                    ImGui::CloseCurrentPopup();
                    data.state = WINDOW_STATE_FILEBROWSER;
                }
                else {
                    // Check for recursive move before attempting
                    if (FS::WouldCauseRecursiveCopy()) {
                        show_recursive_error = true;
                    }
                    // Check if destination already exists (single file only)
                    else if (g_selection.Count() <= 1 && FS::DestinationExists()) {
                        pending_replace_is_move = true;
                        ImGui::CloseCurrentPopup();
                        data.state = WINDOW_STATE_REPLACE;
                    }
                    // Multiple files - check for conflicts
                    else if (g_selection.Count() > 1) {
                        multi_conflict_count = FS::CountConflicts();
                        if (multi_conflict_count > 0) {
                            // Has conflicts - show multi-replace popup
                            multi_total_count = g_selection.Count();
                            Options::pending_multi_move = true;
                            pending_replace_is_move = true;
                            ImGui::CloseCurrentPopup();
                            data.state = WINDOW_STATE_MULTI_REPLACE;
                        }
                        else {
                            // No conflicts - proceed with move
                            Options::HandleMultipleCopy(data, std::addressof(FS::Move), true);
                            move = !move;
                            ImGui::CloseCurrentPopup();
                            data.state = WINDOW_STATE_FILEBROWSER;
                        }
                    }
                    else {
                        if (FS::Move()) {
                            Options::RefreshEntries(true);
                            sort = -1;
                        }
                        
                        move = !move;
                        ImGui::CloseCurrentPopup();
                        data.state = WINDOW_STATE_FILEBROWSER;
                    }
                }
            }
            
            ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
            
            if (ImGui::Button(strings[lang][Lang::OptionsDelete], ImVec2(200, 50))) {
                ImGui::CloseCurrentPopup();
                data.state = WINDOW_STATE_DELETE;
            }
            
            ImGui::SameLine(0.0f, 15.0f);
            
            if (ImGui::Button(strings[lang][Lang::OptionsSetArchiveBit], ImVec2(200, 50))) {
                std::string path = FS::BuildPath(data.entries[data.selected]);
                
                if (FS::SetArchiveBit(path)) {
                    Options::RefreshEntries(true);
                    sort = -1;
                }
                    
                ImGui::CloseCurrentPopup();
                data.state = WINDOW_STATE_FILEBROWSER;
            }
        }
        
        Popups::ExitPopup();
        
        // Show recursive copy error popup if triggered
        if (show_recursive_error) {
            ImGui::OpenPopup("###RecursiveCopyError");
            show_recursive_error = false;
        }
        
        ImGui::SetNextWindowPos(ImVec2(640.0f, 360.0f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("###RecursiveCopyError", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar)) {
            ImGui::Text("%s", strings[lang][Lang::OptionsRecursiveCopyError]);
            ImGui::Dummy(ImVec2(0.0f, 5.0f));
            
            float button_width = 120.0f;
            float window_width = ImGui::GetWindowSize().x;
            ImGui::SetCursorPosX((window_width - button_width) * 0.5f);
            
            if (ImGui::Button(strings[lang][Lang::ButtonOK], ImVec2(button_width, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    bool IsPendingReplaceMove(void) {
        return pending_replace_is_move;
    }
    
    size_t GetMultiConflictCount(void) {
        return multi_conflict_count;
    }
    
    size_t GetMultiTotalCount(void) {
        return multi_total_count;
    }
    
    void ClearPendingMultiOperation(void) {
        Options::pending_multi_copy = false;
        Options::pending_multi_move = false;
        FS::ClearConflictHandling();
    }
}
