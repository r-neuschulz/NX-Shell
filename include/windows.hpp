#pragma once

#include <string>
#include <switch.h>
#include <vector>
#include <mutex>

#include "services.hpp"

// Type alias - map to services.hpp type
typedef WindowService WindowData;

namespace FileBrowser {
    bool Sort(FileSystemService &fs_svc, WindowService &win_svc, const FsDirectoryEntry &entryA, const FsDirectoryEntry &entryB);
}

namespace ImageViewer {
    void ClearTextures(App &app);
    void ClearPreloadedTextures(App &app);
    void CleanupDeferredDeletions(void);
    bool HandleScroll(App &app, int index);
    bool HandlePrev(App &app);
    bool HandleNext(App &app);
    void HandleControls(App &app, u64 &key, bool &properties);
    void PreloadAdjacentImages(App &app);
    int FindPrevImageIndex(App &app, int from_index);
    int FindNextImageIndex(App &app, int from_index);
}

namespace TextReader {
    bool LoadFile(App &app, const std::string &path, bool restore_offset = false);
    void Clear(App &app);
    bool HandleScroll(App &app, int index, bool restore_offset = false);
    bool HandlePrev(App &app);
    bool HandleNext(App &app);
    void HandleControls(App &app, u64 &key, bool &properties);
    float GetScrollY(void);
    void SetScrollY(float y);
    void SetMaxScrollY(float max_y);
    void SaveScrollOffset(void);
    
    bool IsHexMode(void);
    void SetHexMode(bool mode);
    void ToggleHexMode(void);
    const std::vector<unsigned char>& GetRawContent(void);
}

namespace Windows {
    void SetupWindow(GUIService &gui_svc);
    void ExitWindow(void);
    void MainWindow(App &app, u64 &key, bool progress);
    void ImageViewer(App &app, bool &properties, bool &file_stat);
    void TextReader(App &app, bool &properties, bool &file_stat);
    int GetActiveTab(void);
}
