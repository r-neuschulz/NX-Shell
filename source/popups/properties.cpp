#include "config.hpp"
#include "fs.hpp"
#include "gui.hpp"
#include "services.hpp"
#include "imgui.h"
#include "language.hpp"
#include "log.hpp"
#include "popups.hpp"
#include "utils.hpp"

namespace Popups {
    static std::size_t size = 0;

    static char *FormatDate(char *string, time_t timestamp) {
        strftime(string, 36, "%Y-%m-%d %H:%M:%S", localtime(std::addressof(timestamp)));
        return string;
    }

    void FilePropertiesPopup(App &app, bool &file_stat, bool *properties) {
        WindowData &data = app.window;
        const int lang = app.config.Lang();
        Popups::SetupPopup(app, strings[lang][Lang::OptionsProperties]);
        
        if (ImGui::BeginPopupModal(strings[lang][Lang::OptionsProperties], nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar)) {
            std::string name_text = strings[lang][Lang::PropertiesName] + std::string(data.entries[data.selected].name);
            ImGui::Text(name_text.c_str());
            
            if (data.entries[data.selected].type == FsDirEntryType_File) {
                if (!file_stat) {
                    if (!FS::GetFileSize(data.entries[data.selected].name, size)) {
                        Log::Error("FilePropertiesPopup: Failed to get file size for %s\n", data.entries[data.selected].name);
                        size = 0;  // Default to 0 on failure
                    }
                    file_stat = true;
                }

                char size_str[16];
                Utils::GetSizeString(size_str, static_cast<double>(size));
                std::string size_text = strings[lang][Lang::PropertiesSize];
                size_text.append(size_str);
                ImGui::Text(size_text.c_str());
            }
            
            FsTimeStampRaw timestamp;
            if (FS::GetTimeStamp(app.fs, data.entries[data.selected], timestamp)) {
                if (timestamp.is_valid == 1) { // Confirm valid timestamp
                    char date[3][36];
                    
                    std::string created_time = strings[lang][Lang::PropertiesCreated];
                    created_time.append(Popups::FormatDate(date[0], timestamp.created));
                    ImGui::Text(created_time.c_str());
                    
                    std::string modified_time = strings[lang][Lang::PropertiesModified];
                    modified_time.append(Popups::FormatDate(date[1], timestamp.modified));
                    ImGui::Text(modified_time.c_str());
                    
                    std::string accessed_time = strings[lang][Lang::PropertiesAccessed];
                    accessed_time.append(Popups::FormatDate(date[2], timestamp.accessed));
                    ImGui::Text(accessed_time.c_str());
                }
            }
            
            ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
            
            // Center the OK button
            float button_width = 120.0f;
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - button_width) * 0.5f);
            if (ImGui::Button(strings[lang][Lang::ButtonOK], ImVec2(button_width, 36))) {
                file_stat = false;
                // Reset the properties flag when called from text/image viewer
                if (properties)
                    *properties = false;
                ImGui::CloseCurrentPopup();
                // Only return to OPTIONS if called from file browser properties context
                // When called from image/text viewer, state should remain unchanged
                if (data.state == WINDOW_STATE_PROPERTIES) {
                    data.state = WINDOW_STATE_OPTIONS;
                }
            }
            ImGui::SetItemDefaultFocus();
        }
        
        Popups::ExitPopup();
    }

    void ImageProperties(App &app, bool &state, Tex &texture, bool &file_stat) {
        const int lang = app.config.Lang();
        Popups::SetupPopup(app, strings[lang][Lang::OptionsProperties]);

        std::string new_width, new_height;
        if (ImGui::BeginPopupModal(strings[lang][Lang::OptionsProperties], std::addressof(state), ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar)) {
            std::string parent_text = strings[lang][Lang::PropertiesName];
            parent_text.append(app.fs.device);
            parent_text.append(app.fs.cwd);
            ImGui::Text("%s", parent_text.c_str());

            ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing

            std::string name_text = strings[lang][Lang::PropertiesName];
            name_text.append(app.window.entries[app.window.selected].name);
            ImGui::Text("%s", name_text.c_str());

            ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
            
            if (!file_stat) {
                if (!FS::GetFileSize(app.window.entries[app.window.selected].name, size)) {
                    Log::Error("ImageProperties: Failed to get file size for %s\n", app.window.entries[app.window.selected].name);
                    size = 0;  // Default to 0 on failure
                }
                file_stat = true;
            }

            char size_str[16];
            Utils::GetSizeString(size_str, static_cast<double>(size));
            std::string size_text = strings[lang][Lang::PropertiesSize];
            size_text.append(size_str);
            ImGui::Text("%s", size_text.c_str());

            ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing

            std::string width_text = strings[lang][Lang::PropertiesWidth];
            width_text.append(std::to_string(texture.width));
            width_text.append("px");
            ImGui::Text("%s", width_text.c_str());

            ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing

            std::string height_text = strings[lang][Lang::PropertiesHeight];
            height_text.append(std::to_string(texture.height));
            height_text.append("px");
            ImGui::Text("%s", height_text.c_str());

            ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
            
            // Center the OK button
            float button_width = 120.0f;
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - button_width) * 0.5f);
            if (ImGui::Button(strings[lang][Lang::ButtonOK], ImVec2(button_width, 36))) {
                state = false;
                file_stat = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SetItemDefaultFocus();
        }
        
        Popups::ExitPopup();
    }
}
