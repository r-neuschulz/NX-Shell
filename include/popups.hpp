#pragma once

#include <string>

#include "imgui.h"
#include "windows.hpp"

namespace Popups {
    inline void SetupPopup(const char *id) {
        ImGui::OpenPopup(id);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(15, 15));
        ImGui::SetNextWindowPos(ImVec2(640.0f, 360.0f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    };
    
    inline void ExitPopup(void) {
        ImGui::EndPopup();
        ImGui::PopStyleVar();
    };

    void ArchivePopup(void);
    void DeletePopup(WindowData &data);
    void FilePropertiesPopup(WindowData &data, bool &file_stat, bool *properties = nullptr);
    void ImageProperties(bool &state, Tex &texture, bool &file_stat);
    void OptionsPopup(WindowData &data);
    void ReplacePopup(WindowData &data, bool is_move);
    void MultiReplacePopup(WindowData &data, bool is_move, size_t conflict_count, size_t total_count);
    bool IsPendingReplaceMove(void);
    size_t GetMultiConflictCount(void);
    size_t GetMultiTotalCount(void);
    void ClearPendingMultiOperation(void);
    void UpdatePopup(bool &state, bool &connection_status, bool &available, const std::string &tag);
    void ProgressBar(float offset, float size, const std::string &title, const std::string &text);
    void USBPopup(bool &state);
    
    // Open mode popup for unknown/binary files
    // Returns: 0 = cancelled/pending, 1 = open as text, 2 = open as hex
    int OpenModePopup(bool &show);
}
