#pragma once

#include <string>
#include <switch.h>
#include <vector>
#include <mutex>

#include "services.hpp"

// Legacy type aliases - map to services.hpp types
typedef WindowState WINDOW_STATES;
typedef SortState FS_SORT_STATE;
typedef WindowService WindowData;

// Legacy extern declarations - forward to App instance
extern WindowData& data;
extern int& sort;
extern std::vector<std::string>& devices_list;
extern std::recursive_mutex& devices_list_mutex;

namespace FileBrowser {
    bool Sort(const FsDirectoryEntry &entryA, const FsDirectoryEntry &entryB);
}

namespace ImageViewer {
    void ClearTextures(void);
    void ClearPreloadedTextures(void);
    void CleanupDeferredDeletions(void);
    bool HandleScroll(int index);
    bool HandlePrev(void);
    bool HandleNext(void);
    void HandleControls(u64 &key, bool &properties);
    void PreloadAdjacentImages(void);
    int FindPrevImageIndex(int from_index);
    int FindNextImageIndex(int from_index);
}

namespace TextReader {
    bool LoadFile(const std::string &path, bool restore_offset = false);
    void Clear(void);
    bool HandleScroll(int index, bool restore_offset = false);
    bool HandlePrev(void);
    bool HandleNext(void);
    void HandleControls(u64 &key, bool &properties);
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
    void SetupWindow(void);
    void ExitWindow(void);
    void MainWindow(WindowData &data, u64 &key, bool progress);
    void ImageViewer(bool &properties, bool &file_stat);
    void TextReader(bool &properties, bool &file_stat);
    int GetActiveTab(void);
}
