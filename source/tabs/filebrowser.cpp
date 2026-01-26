#include <algorithm>
#include <cstring>

#include "archive.hpp"
#include "bottombar.hpp"
#include "config.hpp"
#include "fs.hpp"
#include "gui.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "language.hpp"
#include "log.hpp"
#include "selection.hpp"
#include "tabs.hpp"
#include "textures.hpp"
#include "utils.hpp"
#include "windows.hpp"

int sort = 0;
std::vector<std::string> devices_list = { "sdmc:", "safe:", "user:", "system:" };
std::recursive_mutex devices_list_mutex;
static std::string pending_focus_name;  // Name of entry to focus after navigation (empty = none)
static std::string current_focused_name;  // Track current focused entry name for table ID transitions
static bool go_to_partition_root = false;
static bool go_to_parent_directory = false;
static bool prev_at_partition_root = false;  // Track previous partition root state
static bool prev_show_details = false;  // Track previous details state

namespace Tabs {
    void RequestFileBrowserFocus(void) {
        // Focus appropriate entry based on current view
        // At partition root: focus "sdmc:" (SD card)
        // In directory: focus ".." (parent navigation)
        pending_focus_name = FS::IsAtPartitionRoot() ? "sdmc:" : "..";
    }
    
    void RequestDeviceCombo(void) {
        // Now navigates to partition root instead of opening combo
        go_to_partition_root = true;
    }
    
    void RequestParentDirectory(void) {
        go_to_parent_directory = true;
    }
    
    void ToggleDetails(void) {
        if (!GUI::IsAppletMode()) {  // No-op in applet mode
            cfg.show_details = !cfg.show_details;
            Config::Save(cfg);
        }
        // Preserve current focus when toggling details (table ID changes)
        if (!current_focused_name.empty()) {
            pending_focus_name = current_focused_name;
        }
    }
    
    bool IsShowingDetails(void) {
        return eff.show_details;
    }
}

namespace FileBrowser {
    // Sort without using ImGuiTableSortSpecs
    bool Sort(const FsDirectoryEntry &entryA, const FsDirectoryEntry &entryB) {
        // Make sure ".." stays at the top regardless of sort direction
        if (strcasecmp(entryA.name, "..") == 0)
            return true;
        
        if (strcasecmp(entryB.name, "..") == 0)
            return false;
        
        if ((entryA.type == FsDirEntryType_Dir) && !(entryB.type == FsDirEntryType_Dir))
            return true;
        else if (!(entryA.type == FsDirEntryType_Dir) && (entryB.type == FsDirEntryType_Dir))
            return false;

        switch(sort) {
            case FS_SORT_ALPHA_ASC:
                return (strcasecmp(entryA.name, entryB.name) < 0);
                break;

            case FS_SORT_ALPHA_DESC:
                return (strcasecmp(entryB.name, entryA.name) < 0);
                break;
        }

        return false;
    }

    // Find alphabetically-first folder name (or first file if no folders)
    // Used to determine focus target after navigation
    std::string GetFirstContentName(const std::vector<FsDirectoryEntry> &entries) {
        const char* best_folder = nullptr;
        const char* best_file = nullptr;
        
        for (const auto &entry : entries) {
            if (std::strncmp(entry.name, "..", 2) == 0)
                continue;
            
            if (entry.type == FsDirEntryType_Dir) {
                if (!best_folder || strcasecmp(entry.name, best_folder) < 0)
                    best_folder = entry.name;
            } else {
                if (!best_file || strcasecmp(entry.name, best_file) < 0)
                    best_file = entry.name;
            }
        }
        
        // Prefer folder over file, fall back to ".." if nothing else
        if (best_folder) return best_folder;
        if (best_file) return best_file;
        return "..";
    }
}

namespace Tabs {
    static const ImVec2 tex_size = ImVec2(21, 21);

    // Helper to draw tinted image (ImGui::Image doesn't support tint in this version)
    static void TintedImage(GLuint texture_id, const ImVec2 &size, ImU32 tint_col) {
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddImage(
            static_cast<ImTextureID>(texture_id),
            pos,
            ImVec2(pos.x + size.x, pos.y + size.y),
            ImVec2(0, 0), ImVec2(1, 1),
            tint_col
        );
        ImGui::Dummy(size);  // Advance cursor
    }
    
    void FileBrowser(WindowData &data, int &current_tab, int &active_tab) {
        ImGuiTabItemFlags flags = (current_tab == 0) ? ImGuiTabItemFlags_SetSelected : 0;
        if (current_tab == 0) current_tab = -1; // Reset after applying
        
        if (ImGui::BeginTabItem(strings[Config::GetLang()][Lang::TabFiles], nullptr, flags)) {
            active_tab = 0;  // Update active tab when this tab is visible
            
            // When tab is clicked/touched, focus the first table entry instead of staying on the tab
            // This allows pressing down to select the first file, not the table header
            if (ImGui::IsItemActivated()) {
                pending_focus_name = FS::IsAtPartitionRoot() ? "sdmc:" : "..";
            }
            
            ImGui::Dummy(ImVec2(0.0f, 1.0f)); // Spacing

            // Handle + button request to go to partition root (always refreshes)
            if (go_to_partition_root) {
                go_to_partition_root = false;
                
                // Start visual feedback animation when refreshing at partition root
                if (FS::IsAtPartitionRoot()) {
                    GUI::StartRefreshAnimation();
                }
                
                // Always refresh when going to partition root
                FS::GoToPartitionRoot(data.entries);
                data.metadata_cache.clear();  // No metadata for partition entries
                data.used_storage = 0;
                data.total_storage = 0;
                pending_focus_name = "sdmc:";  // Default to SD card
                sort = -1;
            }
            
            // Handle B button request to go to parent directory
            if (go_to_parent_directory) {
                go_to_parent_directory = false;
                
                if (FS::IsAtPartitionRoot()) {
                    // Already at partition root - do nothing
                }
                else if (FS::ChangeDirPrev(data.entries)) {
                    FS::PopulateMetadataCache(data.entries, data.metadata_cache);
                    pending_focus_name = "..";  // Focus ".." when going back
                    sort = -1;
                }
                else {
                    // At device root - go to partition root
                    FS::GoToPartitionRoot(data.entries);
                    data.metadata_cache.clear();  // No metadata for partition entries
                    data.used_storage = 0;
                    data.total_storage = 0;
                    pending_focus_name = "sdmc:";  // Default to SD card
                    sort = -1;
                }
            }

            // Display full path (or "Select Device" at partition root)
            std::string display_path = FS::GetDisplayPath();
            if (FS::IsAtPartitionRoot()) {
                ImGui::TextDisabled("%s", strings[Config::GetLang()][Lang::FileBrowserSelectDevice]);
            } else {
                ImGui::Text("%s", display_path.c_str());
            }
            
            // Draw storage bar (empty at partition root, filled otherwise)
            ImGui::Dummy(ImVec2(0.0f, 1.0f)); // Spacing
            if (!FS::IsAtPartitionRoot() && data.total_storage > 0) {
                ImGui::ProgressBar(static_cast<float>(data.used_storage) / static_cast<float>(data.total_storage), ImVec2(ImGui::GetContentRegionAvail().x, 6.0f), "");
            } else {
                ImGui::ProgressBar(0.0f, ImVec2(ImGui::GetContentRegionAvail().x, 6.0f), "");  // Empty bar at partition root
            }
            ImGui::Dummy(ImVec2(0.0f, 1.0f)); // Spacing

            ImGuiTableFlags tableFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_BordersInner |
                ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY;
            
            // Reserve space for button hints at the bottom
            const float button_bar_height = 40.0f;
            float available_height = ImGui::GetContentRegionAvail().y - button_bar_height;
            
            // Column setup depends on whether we're at partition root or in a directory
            bool at_partition_root_for_columns = FS::IsAtPartitionRoot();
            bool show_details = eff.show_details;
            bool show_details_columns = show_details && !at_partition_root_for_columns;
            bool show_usage_column = show_details && at_partition_root_for_columns;
            // At partition root: 2 or 3 columns depending on details (checkbox + device [+ usage bar])
            // Normal view: 2 or 5 columns depending on details setting (checkbox + name [+ size + modified + archive])
            const int column_count = at_partition_root_for_columns ? (show_usage_column ? 3 : 2) : (show_details_columns ? 5 : 2);
            
            // Detect table configuration changes - when switching between partition root and directory
            // we need to preserve the focus since table IDs are different
            bool table_config_changed = (at_partition_root_for_columns != prev_at_partition_root);
            if (table_config_changed && !current_focused_name.empty() && pending_focus_name.empty()) {
                pending_focus_name = current_focused_name;
            }
            prev_at_partition_root = at_partition_root_for_columns;
            prev_show_details = show_details;
            
            ImGuiContext& g = *GImGui;
            
            // Use different table IDs for each distinct column configuration to prevent
            // cached column widths from bleeding between modes when toggling details or navigating
            const char* table_id = at_partition_root_for_columns
                ? (show_usage_column ? "##DirListPRDet" : "##DirListPR")
                : (show_details_columns ? "##DirListDet" : "##DirListNoDet");
            
            if (ImGui::BeginTable(table_id, column_count, tableFlags, ImVec2(0.0f, available_height))) {
                // Make header always visible
                // ImGui::TableSetupScrollFreeze(0, 1);

                // Column 0 (checkbox): At partition root make completely non-interactive, in directories allow sorting
                ImGuiTableColumnFlags checkbox_flags = ImGuiTableColumnFlags_NoHeaderLabel | ImGuiTableColumnFlags_WidthFixed;
                if (at_partition_root_for_columns) {
                    // At partition root: make checkbox column completely non-interactive
                    // NoSort + NoReorder + NoResize + NoHide makes header cell inert
                    checkbox_flags |= ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_NoReorder | ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_NoHide;
                }
                ImGui::TableSetupColumn("", checkbox_flags);
                if (show_usage_column) {
                    // Partition root with details: Device 60%, Usage 40% - Usage is sortable by %
                    ImGui::TableSetupColumn(strings[Config::GetLang()][Lang::FileBrowserDevice], ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthStretch, 0.60f);
                    ImGui::TableSetupColumn("Usage", ImGuiTableColumnFlags_WidthStretch, 0.40f);
                } else if (at_partition_root_for_columns) {
                    // Partition root without details: Device takes full width
                    ImGui::TableSetupColumn(strings[Config::GetLang()][Lang::FileBrowserDevice], ImGuiTableColumnFlags_DefaultSort);
                } else if (show_details_columns) {
                    // Details view: Filename 50%, Size 15%, Date 25%, Archive 10% - Archive is sortable
                    ImGui::TableSetupColumn(strings[Config::GetLang()][Lang::FileBrowserFilename], ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthStretch, 0.50f);
                    ImGui::TableSetupColumn(strings[Config::GetLang()][Lang::FileBrowserSize], ImGuiTableColumnFlags_WidthStretch, 0.15f);
                    ImGui::TableSetupColumn(strings[Config::GetLang()][Lang::FileBrowserModified], ImGuiTableColumnFlags_WidthStretch, 0.25f);
                    ImGui::TableSetupColumn(strings[Config::GetLang()][Lang::FileBrowserArchive], ImGuiTableColumnFlags_WidthStretch, 0.10f);
                } else {
                    // No details: Filename takes full width
                    ImGui::TableSetupColumn(strings[Config::GetLang()][Lang::FileBrowserFilename], ImGuiTableColumnFlags_DefaultSort);
                }
                // Disable keyboard/gamepad navigation on the header row so that pressing down
                // goes directly to the first table entry, not the sort headers
                ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
                ImGui::TableHeadersRow();
                ImGui::PopItemFlag();

                // Sorting logic - handles both directory view and partition root
                // Column indices are the same in both views (checkbox column exists in both):
                //   Both views: column 0 = checkbox (NoSort at partition root), column 1 = device/filename
                //   Partition root: column 2 = usage
                //   Directory view: column 2 = size, column 3 = modified, column 4 = archive
                if (ImGuiTableSortSpecs *sorts_specs = ImGui::TableGetSortSpecs()) {
                    bool is_at_partition_root = FS::IsAtPartitionRoot();
                    bool reset_to_default = (sort == -1);
                    if (reset_to_default) {
                        // Reset visual sort indicator to device/filename column (column 1), ascending
                        ImGui::TableSetColumnSortDirection(1, ImGuiSortDirection_Ascending, false);
                        sorts_specs->SpecsDirty = true;
                    }
                    
                    if (sorts_specs->SpecsDirty) {
                        // Build index mapping for metadata cache lookup after sort
                        std::vector<std::pair<size_t, size_t>> index_map;  // (original_index, entry)
                        for (size_t i = 0; i < data.entries.size(); i++) {
                            index_map.push_back({i, i});
                        }
                        
                        // Get sort specs before lambda
                        // When navigating to new folder (sort == -1), reset to device/filename ascending
                        int sort_column = 1;  // Default to device/filename (column 1)
                        bool descending = false;
                        if (!reset_to_default && sorts_specs->SpecsCount > 0) {
                            sort_column = sorts_specs->Specs[0].ColumnIndex;
                            descending = (sorts_specs->Specs[0].SortDirection == ImGuiSortDirection_Descending);
                        }
                        
                        // Update global sort variable for filename sorting
                        if (sort_column == 1) {
                            sort = descending ? FS_SORT_ALPHA_DESC : FS_SORT_ALPHA_ASC;
                        }
                        
                        // Sort indices using cached metadata
                        auto &entries = data.entries;
                        auto &cache = data.metadata_cache;
                        
                        // For partition root sorting by usage, we need to precompute usage ratios
                        // At partition root: column 2 is usage
                        std::vector<float> usage_ratios;
                        if (is_at_partition_root && sort_column == 2) {
                            usage_ratios.resize(entries.size(), 0.0f);
                            std::scoped_lock lock(::devices_list_mutex);
                            for (size_t i = 0; i < entries.size(); i++) {
                                std::string partition_name = entries[i].name;
                                for (std::size_t pi = 0; pi < ::devices_list.size(); pi++) {
                                    if (::devices_list[pi] == partition_name) {
                                        FsFileSystem *part_fs = std::addressof(devices[pi]);
                                        s64 free_space = 0, total_space = 0;
                                        if (R_SUCCEEDED(fsFsGetFreeSpace(part_fs, "/", &free_space)) &&
                                            R_SUCCEEDED(fsFsGetTotalSpace(part_fs, "/", &total_space)) && total_space > 0) {
                                            usage_ratios[i] = static_cast<float>(total_space - free_space) / static_cast<float>(total_space);
                                        }
                                        break;
                                    }
                                }
                            }
                        }
                        
                        std::sort(index_map.begin(), index_map.end(), 
                            [&entries, &cache, &usage_ratios, sort_column, descending, is_at_partition_root](const std::pair<size_t, size_t> &a, const std::pair<size_t, size_t> &b) {
                                const FsDirectoryEntry &entryA = entries[a.first];
                                const FsDirectoryEntry &entryB = entries[b.first];
                                
                                // Make sure ".." stays at the top regardless of sort direction (only in directory view)
                                if (!is_at_partition_root) {
                                    if (strcasecmp(entryA.name, "..") == 0)
                                        return true;
                                    if (strcasecmp(entryB.name, "..") == 0)
                                        return false;
                                    
                                    // Directories before files (only in directory view)
                                    if ((entryA.type == FsDirEntryType_Dir) && !(entryB.type == FsDirEntryType_Dir))
                                        return true;
                                    if (!(entryA.type == FsDirEntryType_Dir) && (entryB.type == FsDirEntryType_Dir))
                                        return false;
                                }
                                
                                // Handle partition root sorting (col 0 = checkbox/NoSort, col 1 = device, col 2 = usage)
                                if (is_at_partition_root) {
                                    switch (sort_column) {
                                        case 1: // Device name
                                            return descending ? (strcasecmp(entryB.name, entryA.name) < 0) 
                                                              : (strcasecmp(entryA.name, entryB.name) < 0);
                                        
                                        case 2: // Usage percentage
                                            {
                                                float ratioA = (a.first < usage_ratios.size()) ? usage_ratios[a.first] : 0.0f;
                                                float ratioB = (b.first < usage_ratios.size()) ? usage_ratios[b.first] : 0.0f;
                                                if (ratioA != ratioB)
                                                    return descending ? (ratioA > ratioB) : (ratioA < ratioB);
                                            }
                                            // Fall through to device name for stable sort
                                            return descending ? (strcasecmp(entryB.name, entryA.name) < 0) 
                                                              : (strcasecmp(entryA.name, entryB.name) < 0);
                                        
                                        default:
                                            return descending ? (strcasecmp(entryB.name, entryA.name) < 0) 
                                                              : (strcasecmp(entryA.name, entryB.name) < 0);
                                    }
                                }
                                
                                // Handle directory view sorting (col 0 = checkbox, col 1 = filename, etc.)
                                // Build base path for selection lookups
                                std::string base_path = device + cwd;
                                if (!base_path.empty() && base_path.back() != '/')
                                    base_path += "/";
                                
                                switch (sort_column) {
                                    case 0: // Selected (checkbox) - query g_selection
                                        {
                                            std::string pathA = base_path + entryA.name;
                                            std::string pathB = base_path + entryB.name;
                                            bool checkedA = g_selection.IsSelected(pathA);
                                            bool checkedB = g_selection.IsSelected(pathB);
                                            if (checkedA != checkedB)
                                                return descending ? (checkedA < checkedB) : (checkedA > checkedB);  // Checked items first by default
                                        }
                                        // Fall through to filename for stable sort
                                        return descending ? (strcasecmp(entryB.name, entryA.name) < 0) 
                                                          : (strcasecmp(entryA.name, entryB.name) < 0);
                                    
                                    case 1: // filename
                                        return descending ? (strcasecmp(entryB.name, entryA.name) < 0) 
                                                          : (strcasecmp(entryA.name, entryB.name) < 0);
                                    
                                    case 2: // size (only meaningful for files)
                                        if (entryA.type == FsDirEntryType_File && entryB.type == FsDirEntryType_File) {
                                            size_t sizeA = (a.first < cache.size() && cache[a.first].valid) ? cache[a.first].file_size : 0;
                                            size_t sizeB = (b.first < cache.size() && cache[b.first].valid) ? cache[b.first].file_size : 0;
                                            if (sizeA != sizeB)
                                                return descending ? (sizeA > sizeB) : (sizeA < sizeB);
                                        }
                                        // Fall through to filename for stable sort when sizes are equal or both dirs
                                        return descending ? (strcasecmp(entryB.name, entryA.name) < 0) 
                                                          : (strcasecmp(entryA.name, entryB.name) < 0);
                                    
                                    case 3: // modified date
                                        {
                                            bool validA = (a.first < cache.size() && cache[a.first].valid);
                                            bool validB = (b.first < cache.size() && cache[b.first].valid);
                                            if (validA && validB) {
                                                s64 timeA = cache[a.first].modified_time;
                                                s64 timeB = cache[b.first].modified_time;
                                                if (timeA != timeB)
                                                    return descending ? (timeA > timeB) : (timeA < timeB);
                                            }
                                            else if (validA != validB) {
                                                return validA;  // Valid entries come first
                                            }
                                        }
                                        // Fall through to filename for stable sort
                                        return descending ? (strcasecmp(entryB.name, entryA.name) < 0) 
                                                          : (strcasecmp(entryA.name, entryB.name) < 0);
                                    
                                    case 4: // archive bit (only meaningful for directories)
                                        {
                                            // Files don't have archive bit - treat as false
                                            bool archiveA = (entryA.type == FsDirEntryType_Dir && a.first < cache.size() && cache[a.first].valid) ? cache[a.first].has_archive_bit : false;
                                            bool archiveB = (entryB.type == FsDirEntryType_Dir && b.first < cache.size() && cache[b.first].valid) ? cache[b.first].has_archive_bit : false;
                                            if (archiveA != archiveB)
                                                return descending ? (archiveA < archiveB) : (archiveA > archiveB);  // Archive bit set first by default
                                        }
                                        // Fall through to filename for stable sort
                                        return descending ? (strcasecmp(entryB.name, entryA.name) < 0) 
                                                          : (strcasecmp(entryA.name, entryB.name) < 0);
                                    
                                    default:
                                        return descending ? (strcasecmp(entryB.name, entryA.name) < 0) 
                                                          : (strcasecmp(entryA.name, entryB.name) < 0);
                                }
                            });
                        
                        // Reorder entries and cache based on sorted indices
                        std::vector<FsDirectoryEntry> sorted_entries;
                        std::vector<FileMetadataCache> sorted_cache;
                        sorted_entries.reserve(entries.size());
                        sorted_cache.reserve(cache.size());
                        
                        for (const auto &idx : index_map) {
                            sorted_entries.push_back(entries[idx.first]);
                            if (idx.first < cache.size())
                                sorted_cache.push_back(cache[idx.first]);
                        }
                        
                        data.entries = std::move(sorted_entries);
                        data.metadata_cache = std::move(sorted_cache);
                        
                        sorts_specs->SpecsDirty = false;
                    }
                }

                bool at_partition_root = FS::IsAtPartitionRoot();
                
                // Pre-compute base path for selection lookups
                std::string base_path = device + cwd;
                if (!base_path.empty() && base_path.back() != '/')
                    base_path += "/";
                
                for (u64 i = 0; i < data.entries.size(); i++) {
                    ImGui::TableNextRow();

                    // Checkbox column (always present for visual alignment)
                    ImGui::TableNextColumn();
                    ImGui::PushID(i);
                    
                    // Show uncheck icon at partition root (visual only, non-functional)
                    // Show check/uncheck/partcheck based on state in directory view
                    ImU32 accent_tint = GUI::GetAccentColorU32();
                    
                    // Build full path for this item
                    std::string item_path = base_path + data.entries[i].name;
                    
                    // Check if this specific item is selected
                    bool is_checked = !at_partition_root && g_selection.IsSelected(item_path);
                    
                    // Debug: Log path check for first few entries (only once per navigation)
                    static std::string last_debug_base;
                    if (i < 3 && base_path != last_debug_base && g_selection.HasSelections()) {
                        Log::Debug("Display check: base='%s' item='%s' is_checked=%d\n", 
                                   base_path.c_str(), item_path.c_str(), is_checked ? 1 : 0);
                        if (i == 2) last_debug_base = base_path;
                    }
                    
                    // Determine if this item should show partial check
                    bool is_partchecked = false;
                    if (!at_partition_root && !is_checked && g_selection.HasSelections()) {
                        if (std::strncmp(data.entries[i].name, "..", 2) == 0) {
                            // ".." entry: show partcheck if any selections exist that require going "up"
                            // This means: selections are NOT in current folder AND NOT in a descendant
                            is_partchecked = !g_selection.HasSelectionsUnder(base_path);
                        }
                        else if (data.entries[i].type == FsDirEntryType_Dir) {
                            // Subfolder: show partcheck if it has some selections under it
                            // (meaning it's partially selected - some but not all contents selected)
                            is_partchecked = g_selection.HasSelectionsUnder(item_path);
                        }
                    }
                    
                    if (is_checked)
                        TintedImage(check_icon.id, tex_size, accent_tint);
                    else if (is_partchecked)
                        TintedImage(partcheck_icon.id, tex_size, accent_tint);
                    else
                        TintedImage(uncheck_icon.id, tex_size, accent_tint);
                    
                    ImGui::PopID();

                    // Device/filename column
                    ImGui::TableNextColumn();
                    FileType file_type = FS::GetFileType(data.entries[i].name);
                    
                    // Use drive icon at partition root, otherwise folder/file icons
                    // All icons tinted with accent color
                    if (at_partition_root)
                        TintedImage(drive_icon.id, tex_size, accent_tint);
                    else if (data.entries[i].type == FsDirEntryType_Dir)
                        TintedImage(folder_icon.id, tex_size, accent_tint);
                    else
                        TintedImage(file_icons[file_type].id, tex_size, accent_tint);
                    
                    ImGui::SameLine();

                    // Only process Selectable activation if mouse is actually over the item
                    // This prevents touch clicks on bottom button hints from also activating table items
                    // Use default ImGui nav cursor highlighting (no custom corner style)
                    // Don't use SelectOnNav - we want Up/Down to navigate, not activate items
                    bool selectable_activated = ImGui::Selectable(data.entries[i].name, false, ImGuiSelectableFlags_None);
                    ImGuiID item_id = ImGui::GetItemID();
                    ImVec2 item_min = ImGui::GetItemRectMin();
                    ImVec2 item_max = ImGui::GetItemRectMax();
                    ImVec2 mouse_pos = ImGui::GetIO().MousePos;
                    bool mouse_in_item = (mouse_pos.x >= item_min.x && mouse_pos.x <= item_max.x &&
                                          mouse_pos.y >= item_min.y && mouse_pos.y <= item_max.y);
                    
                    // Track current focused item for table ID transitions
                    bool is_nav_focused = (g.NavId == item_id);
                    if (is_nav_focused) {
                        current_focused_name = data.entries[i].name;
                    }
                    
                    // Allow activation if: mouse is in item bounds, OR if it wasn't a mouse click (gamepad/keyboard)
                    bool was_mouse_click = ImGui::GetIO().MouseClicked[0];
                    bool should_activate = selectable_activated && (mouse_in_item || !was_mouse_click);
                    
                    if (should_activate) {
                        if (at_partition_root) {
                            // Selecting a partition - enter that device
                            if (FS::SelectPartition(data.entries[i].name, data.entries)) {
                                FS::PopulateMetadataCache(data.entries, data.metadata_cache);
                                FS::GetUsedStorageSpace(data.used_storage);
                                FS::GetTotalStorageSpace(data.total_storage);
                                pending_focus_name = FileBrowser::GetFirstContentName(data.entries);
                                sort = -1;
                            }
                        }
                        else if (data.entries[i].type == FsDirEntryType_Dir) {
                            if (std::strncmp(data.entries[i].name, "..", 2) == 0) {
                                // Going back
                                if (FS::ChangeDirPrev(data.entries)) {
                                    FS::PopulateMetadataCache(data.entries, data.metadata_cache);
                                    pending_focus_name = "..";
                                }
                                else {
                                    // At device root - go to partition root
                                    FS::GoToPartitionRoot(data.entries);
                                    data.metadata_cache.clear();
                                    data.used_storage = 0;
                                    data.total_storage = 0;
                                    pending_focus_name = "sdmc:";  // Default to SD card
                                }
                                sort = -1;
                            }
                            else if (FS::ChangeDirNext(data.entries[i].name, data.entries)) {
                                // Going deeper
                                FS::PopulateMetadataCache(data.entries, data.metadata_cache);
                                pending_focus_name = FileBrowser::GetFirstContentName(data.entries);
                                sort = -1;
                            }
                        }
                        else {
                            std::string path = FS::BuildPath(data.entries[i]);
                            
                            switch (file_type) {
                                case FileTypeArchive:
                                    Archive::SetArchivePath(path);
                                    data.selected = i;
                                    data.state = WINDOW_STATE_ARCHIVEEXTRACT;
                                    break;

                                case FileTypeImage:
                                    if (Textures::LoadImageFile(path, data.textures)) {
                                        data.selected = i;  // Set selected to the actual image being opened
                                        data.image_fullscreen = eff.enter_images_fullscreen;
                                        data.state = WINDOW_STATE_IMAGEVIEWER;
                                    }
                                    break;

                                case FileTypeText:
                                    if (TextReader::LoadFile(path)) {
                                        data.selected = i;  // Set selected to the actual text file being opened
                                        TextReader::SetHexMode(false);  // Text files open in text mode
                                        data.state = WINDOW_STATE_TEXTREADER;
                                    }
                                    break;

                                case FileTypeBinary:
                                    // Binary files open directly in hex mode
                                    if (TextReader::LoadFile(path)) {
                                        data.selected = i;
                                        TextReader::SetHexMode(true);  // Binary files open in hex mode
                                        data.state = WINDOW_STATE_TEXTREADER;
                                    }
                                    break;

                                case FileTypeNone:
                                default:
                                    // Unknown files - show popup to choose mode
                                    data.selected = i;
                                    data.state = WINDOW_STATE_OPENMODE;
                                    break;
                            }
                        }
                    }

                    if (ImGui::IsItemHovered())
                        data.selected = i;
                    
                    // Size, Modified, and Archive columns (only when details are shown and not at partition root)
                    // Uses cached metadata to avoid per-frame stat() calls
                    if (show_details_columns) {
                        ImGui::TableNextColumn();
                        // Size column - right aligned, only for files
                        {
                            const char* size_text = "-";
                            char size_str[16];
                            // Safety check: ensure cache is populated and index is valid
                            if (i < data.metadata_cache.size()) {
                                const FileMetadataCache &meta = data.metadata_cache[i];
                                if (data.entries[i].type == FsDirEntryType_File && meta.valid) {
                                    Utils::GetSizeString(size_str, static_cast<double>(meta.file_size));
                                    size_text = size_str;
                                }
                            }
                            // Right-align the text
                            float column_width = ImGui::GetColumnWidth();
                            float text_width = ImGui::CalcTextSize(size_text).x;
                            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + column_width - text_width - ImGui::GetStyle().CellPadding.x);
                            ImGui::TextUnformatted(size_text);
                        }
                        
                        ImGui::TableNextColumn();
                        // Modified column - skip ".." and invalid entries
                        if (std::strncmp(data.entries[i].name, "..", 2) != 0 && i < data.metadata_cache.size()) {
                            const FileMetadataCache &meta = data.metadata_cache[i];
                            if (meta.valid) {
                                char date_str[20];
                                time_t mod_time = static_cast<time_t>(meta.modified_time);
                                strftime(date_str, sizeof(date_str), "%Y-%m-%d %H:%M", localtime(&mod_time));
                                ImGui::TextUnformatted(date_str);
                            }
                        }
                        
                        ImGui::TableNextColumn();
                        // Archive bit column - centered, only for directories (not "..")
                        if (data.entries[i].type == FsDirEntryType_Dir && std::strncmp(data.entries[i].name, "..", 2) != 0 && i < data.metadata_cache.size()) {
                            const FileMetadataCache &meta = data.metadata_cache[i];
                            if (meta.valid) {
                                const char* circle = meta.has_archive_bit ? "\xE2\x97\x8F" : "\xE2\x97\x8B";  // ● or ○
                                // Center the text
                                float column_width = ImGui::GetColumnWidth();
                                float text_width = ImGui::CalcTextSize(circle).x;
                                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (column_width - text_width) * 0.5f - ImGui::GetStyle().CellPadding.x);
                                ImGui::TextUnformatted(circle);
                            }
                        }
                    }
                    
                    // Usage bar column (only at partition root when details are shown)
                    if (show_usage_column) {
                        ImGui::TableNextColumn();
                        
                        // Get storage info for this partition
                        s64 used = 0, total = 0;
                        std::string partition_name = data.entries[i].name;
                        
                        // Find the partition index and get its storage info
                        std::scoped_lock lock(::devices_list_mutex);
                        for (std::size_t pi = 0; pi < ::devices_list.size(); pi++) {
                            if (::devices_list[pi] == partition_name) {
                                // Temporarily query this partition's storage
                                FsFileSystem *part_fs = std::addressof(devices[pi]);
                                s64 free_space = 0, total_space = 0;
                                if (R_SUCCEEDED(fsFsGetFreeSpace(part_fs, "/", &free_space)) &&
                                    R_SUCCEEDED(fsFsGetTotalSpace(part_fs, "/", &total_space))) {
                                    used = total_space - free_space;
                                    total = total_space;
                                }
                                break;
                            }
                        }
                        
                        if (total > 0) {
                            float ratio = static_cast<float>(used) / static_cast<float>(total);
                            
                            // Build size text
                            char size_text[40];  // Enough for "15 chars / 15 chars" + null
                            char used_str[16], total_str[16];
                            Utils::GetSizeString(used_str, static_cast<double>(used));
                            Utils::GetSizeString(total_str, static_cast<double>(total));
                            std::snprintf(size_text, sizeof(size_text), "%s / %s", used_str, total_str);
                            
                            // Draw progress bar as background behind text (fixed cell height)
                            // Use GetColumnWidth() instead of GetContentRegionAvail() for stable sizing
                            // when table column structure changes (avoids cached layout issues)
                            ImDrawList* draw_list = ImGui::GetWindowDrawList();
                            ImVec2 cell_min = ImGui::GetCursorScreenPos();
                            float cell_padding = ImGui::GetStyle().CellPadding.x;
                            float cell_width = ImGui::GetColumnWidth(-1) - cell_padding * 2.0f;
                            float cell_height = ImGui::GetTextLineHeight();
                            ImVec2 cell_max = ImVec2(cell_min.x + cell_width, cell_min.y + cell_height);
                            
                            // Draw background bar (filled portion)
                            ImU32 bar_color = IM_COL32(100, 100, 100, 120);  // Semi-transparent gray
                            ImVec2 bar_max = ImVec2(cell_min.x + cell_width * ratio, cell_max.y);
                            draw_list->AddRectFilled(cell_min, bar_max, bar_color, 2.0f);
                            
                            // Draw text on top of the bar
                            ImGui::TextUnformatted(size_text);
                        } else {
                            ImGui::TextDisabled("N/A");
                        }
                    }
                }

                // Focus by name after navigation - simple and sorting-independent
                if (!pending_focus_name.empty()) {
                    ImGuiWindow* window = ImGui::GetCurrentWindow();
                    ImGui::SetNavWindow(window);
                    ImGui::SetNavID(ImGui::GetID(pending_focus_name.c_str(), 0), g.NavLayer, 0, ImRect());
                    g.NavCursorVisible = true;
                    current_focused_name = pending_focus_name;  // Update tracked name
                    pending_focus_name.clear();
                }

                ImGui::EndTable();
            }

            // Button hints bar at the bottom (right-aligned)
            // Only draw when in file browser state (not when image viewer/text reader is active)
            if (data.state != WINDOW_STATE_FILEBROWSER) {
                ImGui::EndTabItem();
                return;
            }
            
            const int lang = Config::GetLang();
            bool is_at_partition_root = FS::IsAtPartitionRoot();
            
            // Left-aligned items (minus button for exit)
            std::vector<BottomBar::HintItem> left_items = {
                {BottomBar::ButtonType::Minus, strings[lang][Lang::HintExit]}
            };
            
            // Right-aligned items - conditionally show based on partition root state
            std::vector<BottomBar::HintItem> right_items;
            right_items.push_back({BottomBar::ButtonType::CircleA, strings[lang][Lang::HintOpen]});
            
            if (!is_at_partition_root) {
                right_items.push_back({BottomBar::ButtonType::CircleB, strings[lang][Lang::HintBack]});
                right_items.push_back({BottomBar::ButtonType::CircleY, strings[lang][Lang::HintSelect]});
                right_items.push_back({BottomBar::ButtonType::CircleX, strings[lang][Lang::HintOptions]});
            }
            
            // + button: "Refresh" at partition root (with animation), "Drive" elsewhere
            right_items.push_back({BottomBar::ButtonType::Plus, 
                is_at_partition_root ? "Refresh" : strings[lang][Lang::HintDrive],
                is_at_partition_root});  // active = true triggers refresh animation at partition root
            
            // ZR button for details toggle
            right_items.push_back({BottomBar::ButtonType::ShoulderZR, strings[lang][Lang::HintDetails], eff.show_details});
            
            BottomBar::Config config;
            config.use_foreground_draw_list = false;  // Within window
            config.draw_background = false;
            config.hint_spacing = 25.0f;
            
            BottomBar::Draw(config, left_items, right_items);

            ImGui::EndTabItem();
        }
    }
}
