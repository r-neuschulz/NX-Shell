#pragma once

#include <string>

#include "imgui.h"
#include "gui.hpp"
#include "windows.hpp"
#include "services.hpp"

namespace Popups {
    inline void SetupPopup(App &app, const char *id) {
        ImGui::OpenPopup(id);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(15, 15));
        ImGui::SetNextWindowPos(GUI::GetScreenCenter(app), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    };
    
    inline void ExitPopup(void) {
        ImGui::EndPopup();
        ImGui::PopStyleVar();
    };

    void ArchivePopup(App &app);
    void DeletePopup(App &app);
    void FilePropertiesPopup(App &app, bool &file_stat, bool *properties = nullptr);
    void ImageProperties(App &app, bool &state, Tex &texture, bool &file_stat);
    void OptionsPopup(App &app);
    void ReplacePopup(App &app, bool is_move);
    void MultiReplacePopup(App &app, bool is_move, size_t conflict_count, size_t total_count);
    bool IsPendingReplaceMove(void);
    size_t GetMultiConflictCount(void);
    size_t GetMultiTotalCount(void);
    void ClearPendingMultiOperation(FileSystemService &fs_svc);
    
    // Copy/Move mode accessors
    bool IsCopyMode(void);
    void SetCopyMode(bool value);
    bool IsMoveMode(void);
    void SetMoveMode(bool value);
    void UpdatePopup(App &app, bool &state, bool &connection_status, bool &available, const std::string &tag);
    void UpdateWelcomePopup(App &app, bool &state, const std::string &version);
    void ProgressBar(App &app, float offset, float size, const std::string &title, const std::string &text);
    void USBPopup(App &app, bool &state);
    
    // Open mode popup for unknown/binary files
    // Returns: 0 = cancelled/pending, 1 = open as text, 2 = open as hex
    int OpenModePopup(App &app, bool &show);
}
