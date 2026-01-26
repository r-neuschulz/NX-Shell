#pragma once

#include <switch.h>
#include <vector>

#include "imgui.h"
#include "windows.hpp"
#include "services.hpp"

constexpr int TAB_COUNT = 3;

namespace Tabs {
    void FileBrowser(App &app, int &current_tab, int &active_tab);
    void Settings(App &app, int &current_tab, int &active_tab);
    void About(App &app, int &current_tab, int &active_tab);
    
    // Focus request functions - call to request focus when tab is switched
    void RequestFileBrowserFocus(FileSystemService &fs_svc);
    void RequestSettingsFocus(void);
    void RequestAboutFocus(void);
    
    // Request to open the device selector combo
    void RequestDeviceCombo(void);
    
    // Request to navigate to parent directory (..)
    void RequestParentDirectory(void);
    
    // Toggle details view (size, date modified columns)
    void ToggleDetails(App &app);
    
    // Check if details are currently shown
    bool IsShowingDetails(ConfigService &config_svc);
}
