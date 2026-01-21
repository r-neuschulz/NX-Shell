#pragma once

#include <string>
#include <switch.h>
#include <vector>
#include <mutex>

#include "fs.hpp"
#include "selection.hpp"
#include "textures.hpp"

enum WINDOW_STATES {
    WINDOW_STATE_FILEBROWSER = 0,
    WINDOW_STATE_SETTINGS,
    WINDOW_STATE_OPTIONS,
    WINDOW_STATE_DELETE,
    WINDOW_STATE_PROPERTIES,
    WINDOW_STATE_IMAGEVIEWER,
    WINDOW_STATE_ARCHIVEEXTRACT,
    WINDOW_STATE_TEXTREADER,
    WINDOW_STATE_REPLACE,
    WINDOW_STATE_MULTI_REPLACE,
    WINDOW_STATE_OPENMODE  // Popup for choosing text/hex mode for unknown files
};

enum FS_SORT_STATE {
    FS_SORT_ALPHA_ASC = 0,
    FS_SORT_ALPHA_DESC,
    FS_SORT_SIZE_ASC,
    FS_SORT_SIZE_DESC
};

typedef struct {
    WINDOW_STATES state = WINDOW_STATE_FILEBROWSER;
    u64 selected = 0;
    std::vector<FsDirectoryEntry> entries;
    std::vector<FileMetadataCache> metadata_cache;  // Parallel to entries
    s64 used_storage = 0;
    s64 total_storage = 0;
    std::vector<Tex> textures;
    long unsigned int frame_count = 0;
    float zoom_factor = 1.0f;
    // Image viewer pre-loading
    std::vector<Tex> textures_prev;      // Pre-loaded previous image
    std::vector<Tex> textures_next;      // Pre-loaded next image
    int preload_prev_index = -1;         // Index of pre-loaded previous image (-1 = none)
    int preload_next_index = -1;         // Index of pre-loaded next image (-1 = none)
    bool image_fullscreen = false;       // Fullscreen mode (hides bottom bar)
    // Text reader data
    std::string text_content;
} WindowData;

extern WindowData data;
extern int sort;
extern std::vector<std::string> devices_list;
extern std::recursive_mutex devices_list_mutex;

namespace FileBrowser {
    bool Sort(const FsDirectoryEntry &entryA, const FsDirectoryEntry &entryB);
}

namespace ImageViewer {
    void ClearTextures(void);
    void ClearPreloadedTextures(void);
    void CleanupDeferredDeletions(void);  // Call on exit to free any queued textures
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
    
    // Hex mode functions
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
    int GetActiveTab(void);  // Returns current active tab: 0=FileBrowser, 1=Settings, 2=About
}
