#pragma once

#include <switch.h>
#include <vector>

#include "imgui.h"
#include "windows.hpp"

constexpr int TAB_COUNT = 3;

namespace Tabs {
    void FileBrowser(WindowData &data, int &current_tab, int &active_tab);
    void Settings(WindowData &data, int &current_tab, int &active_tab);
    void About(WindowData &data, int &current_tab, int &active_tab);
    
    // Focus request functions - call to request focus when tab is switched
    void RequestFileBrowserFocus(void);
    void RequestSettingsFocus(void);
    void RequestAboutFocus(void);
    
    // Request to open the device selector combo
    void RequestDeviceCombo(void);
    
    // Request to navigate to parent directory (..)
    void RequestParentDirectory(void);
}
