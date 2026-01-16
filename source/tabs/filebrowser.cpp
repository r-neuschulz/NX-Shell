#include <algorithm>
#include <cstring>

#include "config.hpp"
#include "fs.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "language.hpp"
#include "tabs.hpp"
#include "textures.hpp"
#include "utils.hpp"

int sort = 0;
std::vector<std::string> devices_list = { "sdmc:", "safe:", "user:", "system:" };
std::recursive_mutex devices_list_mutex;
static bool need_focus_first_entry = true;
static bool open_device_combo = false;
static bool go_to_parent_directory = false;

namespace Tabs {
    void RequestFileBrowserFocus(void) {
        need_focus_first_entry = true;
    }
    
    void RequestDeviceCombo(void) {
        open_device_combo = true;
    }
    
    void RequestParentDirectory(void) {
        go_to_parent_directory = true;
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

    // Sort using ImGuiTableSortSpecs
    bool TableSort(const FsDirectoryEntry &entryA, const FsDirectoryEntry &entryB) {
        bool descending = false;
        ImGuiTableSortSpecs *table_sort_specs = ImGui::TableGetSortSpecs();
        
        for (int i = 0; i < table_sort_specs->SpecsCount; ++i) {
            const ImGuiTableColumnSortSpecs *column_sort_spec = std::addressof(table_sort_specs->Specs[i]);
            descending = (column_sort_spec->SortDirection == ImGuiSortDirection_Descending);

            // Make sure ".." stays at the top regardless of sort direction
            if (strcasecmp(entryA.name, "..") == 0)
                return true;
            
            if (strcasecmp(entryB.name, "..") == 0)
                return false;
            
            if ((entryA.type == FsDirEntryType_Dir) && !(entryB.type == FsDirEntryType_Dir))
                return true;
            else if (!(entryA.type == FsDirEntryType_Dir) && (entryB.type == FsDirEntryType_Dir))
                return false;
            else {
                switch (column_sort_spec->ColumnIndex) {
                    case 1: // filename
                        sort = descending? FS_SORT_ALPHA_DESC : FS_SORT_ALPHA_ASC;
                        return descending? (strcasecmp(entryB.name, entryA.name) < 0) : (strcasecmp(entryA.name, entryB.name) < 0);
                        break;
                        
                    default:
                        break;
                }
            }
        }
        
        return false;
    }
}

namespace Tabs {
    static const ImVec2 tex_size = ImVec2(21, 21);

    void FileBrowser(WindowData &data, int &current_tab, int &active_tab) {
        ImGuiTabItemFlags flags = (current_tab == 0) ? ImGuiTabItemFlags_SetSelected : 0;
        if (current_tab == 0) current_tab = -1; // Reset after applying
        
        if (ImGui::BeginTabItem("File Browser", nullptr, flags)) {
            active_tab = 0;  // Update active tab when this tab is visible
            
            // Handle B button request to go to parent directory
            if (go_to_parent_directory) {
                go_to_parent_directory = false;
                if (FS::ChangeDirPrev(data.entries)) {
                    if ((data.checkbox_data.count > 1) && (data.checkbox_data.checked_copy.empty()))
                        data.checkbox_data.checked_copy = data.checkbox_data.checked;
                    
                    data.checkbox_data.checked.resize(data.entries.size());
                    need_focus_first_entry = true;
                    sort = -1;
                }
            }
            
            ImGui::Dummy(ImVec2(0.0f, 1.0f)); // Spacing

            ImGui::PushID("device_list");
            ImGui::PushItemWidth(160.f);
            
            // Open the combo programmatically when Plus is pressed
            if (open_device_combo) {
                // Calculate the combo popup ID (same logic as ImGui's BeginCombo)
                ImGuiID popup_id = ImHashStr("##ComboPopup", 0, ImGui::GetID(""));
                ImGui::OpenPopupEx(popup_id, ImGuiPopupFlags_None);
                open_device_combo = false;
            }
            
            if (ImGui::BeginCombo("", device.c_str())) {
                std::scoped_lock lock(devices_list_mutex);

                for (std::size_t i = 0; i < devices_list.size(); i++) {
                    const bool is_selected = (device == devices_list[i]);
                    
                    if (ImGui::Selectable(devices_list[i].c_str(), is_selected)) {
                        device = devices_list[i];
                        fs = std::addressof(devices[i]);
                        
                        cwd = "/";
                        data.entries.clear();
                        FS::GetDirList(device, cwd, data.entries);
                        
                        data.checkbox_data.checked.resize(data.entries.size());
                        FS::GetUsedStorageSpace(data.used_storage);
                        FS::GetTotalStorageSpace(data.total_storage);
                        sort = -1;
                        need_focus_first_entry = true;
                    }
                        
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }

                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();
            ImGui::PopID();
            
            ImGui::SameLine();
            
            // Draw small (+) indicator next to the device dropdown
            {
                ImDrawList *draw_list = ImGui::GetWindowDrawList();
                ImVec2 pos = ImGui::GetCursorScreenPos();
                const float plus_radius = 10.0f;
                const ImU32 plus_bg_color = IM_COL32(80, 80, 80, 255);
                const ImU32 plus_text_color = IM_COL32(255, 255, 255, 255);
                
                ImVec2 center(pos.x + plus_radius, pos.y + ImGui::GetTextLineHeight() * 0.5f);
                draw_list->AddCircleFilled(center, plus_radius, plus_bg_color, 16);
                
                const char* plus_label = "+";
                ImVec2 text_size = ImGui::CalcTextSize(plus_label);
                ImVec2 text_pos(center.x - text_size.x * 0.5f, center.y - text_size.y * 0.5f);
                draw_list->AddText(text_pos, plus_text_color, plus_label);
                
                // Add spacing for the indicator
                ImGui::Dummy(ImVec2(plus_radius * 2 + 8.0f, 0));
                ImGui::SameLine();
            }

            // Display current working directory
            ImGui::Text(cwd.c_str());
            
            // Draw storage bar
            ImGui::Dummy(ImVec2(0.0f, 1.0f)); // Spacing
            ImGui::ProgressBar(static_cast<float>(data.used_storage) / static_cast<float>(data.total_storage), ImVec2(ImGui::GetContentRegionAvail().x, 6.0f), "");
            ImGui::Dummy(ImVec2(0.0f, 1.0f)); // Spacing

            ImGuiTableFlags tableFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_BordersInner |
                ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY;
            
            // Reserve space for button hints at the bottom
            const float button_bar_height = 40.0f;
            float available_height = ImGui::GetContentRegionAvail().y - button_bar_height;
            
            if (ImGui::BeginTable("Directory List", 2, tableFlags, ImVec2(0.0f, available_height))) {
                // Make header always visible
                // ImGui::TableSetupScrollFreeze(0, 1);

                ImGui::TableSetupColumn("", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_NoHeaderLabel | ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn(strings[Config::GetLang()][Lang::FileBrowserFilename], ImGuiTableColumnFlags_DefaultSort);
                ImGui::TableHeadersRow();

                if (ImGuiTableSortSpecs *sorts_specs = ImGui::TableGetSortSpecs()) {
                    if (sort == -1)
                        sorts_specs->SpecsDirty = true;
                    
                    if (sorts_specs->SpecsDirty) {
                        std::sort(data.entries.begin(), data.entries.end(), FileBrowser::TableSort);
                        sorts_specs->SpecsDirty = false;
                    }
                }

                for (u64 i = 0; i < data.entries.size(); i++) {
                    ImGui::TableNextRow();

                    ImGui::TableNextColumn();
                    ImGui::PushID(i);
                    
                    if ((data.checkbox_data.checked[i]) && (data.checkbox_data.cwd.compare(cwd) == 0) && (data.checkbox_data.device.compare(device) == 0))
                        ImGui::Image(static_cast<ImTextureID>(check_icon.id), tex_size);
                    else
                        ImGui::Image(static_cast<ImTextureID>(uncheck_icon.id), tex_size);
                    
                    ImGui::PopID();

                    ImGui::TableNextColumn();
                    FileType file_type = FS::GetFileType(data.entries[i].name);
                    
                    if (data.entries[i].type == FsDirEntryType_Dir)
                        ImGui::Image(static_cast<ImTextureID>(folder_icon.id), tex_size);
                    else
                        ImGui::Image(static_cast<ImTextureID>(file_icons[file_type].id), tex_size);
                    
                    ImGui::SameLine();

                    if (ImGui::Selectable(data.entries[i].name, false)) {
                        if (data.entries[i].type == FsDirEntryType_Dir) {
                            if (std::strncmp(data.entries[i].name, "..", 2) == 0) {
                                if (FS::ChangeDirPrev(data.entries)) {
                                    if ((data.checkbox_data.count > 1) && (data.checkbox_data.checked_copy.empty()))
                                        data.checkbox_data.checked_copy = data.checkbox_data.checked;
                                        
                                    data.checkbox_data.checked.resize(data.entries.size());
                                }
                            }
                            else if (FS::ChangeDirNext(data.entries[i].name, data.entries)) {
                                if ((data.checkbox_data.count > 1) && (data.checkbox_data.checked_copy.empty()))
                                    data.checkbox_data.checked_copy = data.checkbox_data.checked;
                                
                                data.checkbox_data.checked.resize(data.entries.size());
                            }

                            // Reset navigation ID -- TODO: Scroll to top
                            ImGuiContext& g = *GImGui;
                            ImGui::SetNavID(ImGui::GetID(data.entries[0].name, 0), g.NavLayer, 0, ImRect());

                            // Reapply sort
                            ImGuiTableSortSpecs *sorts_specs = ImGui::TableGetSortSpecs();
                            sorts_specs->SpecsDirty = true;
                        }
                        else {
                            std::string path = FS::BuildPath(data.entries[i]);
                            
                            switch (file_type) {
                                case FileTypeImage:
                                    if (Textures::LoadImageFile(path, data.textures))
                                        data.state = WINDOW_STATE_IMAGEVIEWER;
                                    break;

                                default:
                                    break;
                            }
                        }
                    }

                    if (ImGui::IsItemHovered())
                        data.selected = i;
                }

                // Auto-focus first entry on startup or when entering a new directory
                if (need_focus_first_entry && !data.entries.empty()) {
                    ImGuiContext& g = *GImGui;
                    ImGuiWindow* window = ImGui::GetCurrentWindow();
                    ImGui::SetNavWindow(window);
                    ImGui::SetNavID(ImGui::GetID(data.entries[0].name, 0), g.NavLayer, 0, ImRect());
                    g.NavCursorVisible = true;  // Was NavDisableHighlight = false (renamed in ImGui 1.91.4, opposite value)
                    need_focus_first_entry = false;
                }

                ImGui::EndTable();
            }

            // Button hints bar at the bottom (right-aligned)
            ImDrawList *draw_list = ImGui::GetWindowDrawList();
            const float button_radius = 12.0f;
            const float hint_spacing = 25.0f;
            
            // Nintendo Switch button colors
            const ImU32 color_a = IM_COL32(235, 64, 52, 255);    // Red (A)
            const ImU32 color_b = IM_COL32(200, 150, 0, 255);    // Yellow (B) - darkened
            const ImU32 color_y = IM_COL32(60, 140, 30, 255);    // Green (Y) - darkened
            const ImU32 color_x = IM_COL32(65, 137, 230, 255);   // Blue (X)
            const ImU32 color_text = IM_COL32(255, 255, 255, 255);
            const ImU32 color_label = IM_COL32(200, 200, 200, 255);
            
            ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
            float center_y = cursor_pos.y + (button_bar_height * 0.5f);
            
            struct ButtonHint {
                const char* letter;
                const char* label;
                ImU32 color;
            };
            
            const int lang = Config::GetLang();
            const ImU32 color_plus = IM_COL32(80, 80, 80, 255);     // Gray (+)
            
            ButtonHint hints[] = {
                {"A", strings[lang][Lang::HintOpen], color_a},
                {"B", strings[lang][Lang::HintBack], color_b},
                {"Y", strings[lang][Lang::HintSelect], color_y},
                {"X", strings[lang][Lang::HintOptions], color_x},
                {"+", strings[lang][Lang::HintDrive], color_plus}
            };
            
            // Calculate total width of all button hints
            float total_width = 0.0f;
            for (const auto& hint : hints) {
                total_width += button_radius * 2 + 6.0f + ImGui::CalcTextSize(hint.label).x + hint_spacing;
            }
            total_width -= hint_spacing; // Remove trailing spacing
            
            // Start from the right side
            float right_edge = cursor_pos.x + ImGui::GetContentRegionAvail().x - 10.0f;
            float x_offset = right_edge - total_width;
            
            for (const auto& hint : hints) {
                ImVec2 center(x_offset + button_radius, center_y);
                
                // Draw filled circle
                draw_list->AddCircleFilled(center, button_radius, hint.color, 24);
                
                // Draw letter centered in the circle
                ImVec2 text_size = ImGui::CalcTextSize(hint.letter);
                ImVec2 text_pos(center.x - text_size.x * 0.5f, center.y - text_size.y * 0.5f);
                draw_list->AddText(text_pos, color_text, hint.letter);
                
                // Draw label next to the button
                float label_x = x_offset + button_radius * 2 + 6.0f;
                draw_list->AddText(ImVec2(label_x, center_y - ImGui::GetTextLineHeight() * 0.5f), color_label, hint.label);
                
                // Calculate offset for next button
                ImVec2 label_size = ImGui::CalcTextSize(hint.label);
                x_offset = label_x + label_size.x + hint_spacing;
            }

            ImGui::EndTabItem();
        }
    }
}
