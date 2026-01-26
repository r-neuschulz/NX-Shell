#pragma once

#include <mutex>
#include <set>
#include <string>
#include <vector>
#include <switch.h>
#include <glad/glad.h>

// Note: FsDirectoryEntry is a C typedef in libnx, not a C++ struct,
// so it cannot be forward-declared. <switch.h> must be included first.

// ============================================================================
// Resolution and Theme Enums (moved from config.hpp to avoid circular deps)
// ============================================================================

enum ResolutionMode {
    ResolutionMode_Auto = 0,
    ResolutionMode_1080p = 1,
    ResolutionMode_720p = 2
};

enum ThemeMode {
    ThemeMode_Auto = 0,
    ThemeMode_Dark = 1,
    ThemeMode_Light = 2
};

enum ButtonStyle {
    ButtonStyle_Colored = 0,
    ButtonStyle_Mono = 1,
    ButtonStyle_Accent = 2
};

constexpr int LANG_AUTO = -1;

// ============================================================================
// FileSystem Service
// ============================================================================

enum FileSystemDevices {
    FileSystemSDMC = 0,
    FileSystemSafe,
    FileSystemUser,
    FileSystemSystem,
    FileSystemMax
};

enum FileType {
    FileTypeNone,
    FileTypeArchive,
    FileTypeImage,
    FileTypeText,
    FileTypeBinary
};

enum ConflictHandling {
    ConflictHandling_Ask,
    ConflictHandling_ReplaceAll,
    ConflictHandling_SkipAll
};

struct FileMetadataCache {
    std::size_t file_size = 0;
    s64 modified_time = 0;
    bool has_archive_bit = false;
    bool valid = false;
};

struct FileSystemService {
    FsFileSystem *current_fs = nullptr;
    FsFileSystem devices[FileSystemMax] = {};
    std::string cwd = "/";
    std::string device = "sdmc:";
    
    // Copy operation state
    struct CopyEntry {
        std::string path;
        std::string filename;
        bool is_directory = false;
    } copy_entry;
    
    // Conflict handling for multi-file operations
    ConflictHandling conflict_mode = ConflictHandling_Ask;
};

// ============================================================================
// Device Registry Service
// ============================================================================

struct DeviceRegistry {
    std::vector<std::string> devices = {"sdmc:", "safe:", "user:", "system:"};
    std::recursive_mutex mutex;
};

// ============================================================================
// Config Service
// ============================================================================

struct ConfigData {
    int lang = LANG_AUTO;
    bool dev_options = false;
    bool image_filename = false;
    bool enter_images_fullscreen = false;
    int resolution_mode = ResolutionMode_Auto;
    int theme_mode = ThemeMode_Auto;
    bool show_details = false;
    bool show_stats = false;
    std::string last_device = "sdmc:";
    std::string last_cwd = "/";
    float accent_color[3] = {0.0f, 0.50f, 0.50f};
    int button_style = ButtonStyle_Colored;
    
    // Applet mode settings
    bool applet_dev_options = false;
    std::string applet_last_device = "sdmc:";
    std::string applet_last_cwd = "/";
};

struct ConfigService {
    ConfigData saved;      // Saved to disk
    ConfigData effective;  // Runtime effective (read from this!)
};

// ============================================================================
// Selection Service
// ============================================================================

struct SelectionService {
    std::set<std::string> selected_paths;
    std::string device;
    std::string selection_directory;
    
    // Helper methods inline
    bool IsSelected(const std::string &path) const {
        return selected_paths.find(path) != selected_paths.end();
    }
    
    bool HasSelections() const {
        return !selected_paths.empty();
    }
    
    size_t Count() const {
        return selected_paths.size();
    }
    
    void Clear() {
        selected_paths.clear();
        device.clear();
        selection_directory.clear();
    }
};

// ============================================================================
// Texture Service
// ============================================================================

struct Tex {
    GLuint id = 0;
    int width = 0;
    int height = 0;
    int delay = 0;
};

struct TextureService {
    std::vector<Tex> file_icons;
    Tex folder_icon = {};
    Tex check_icon = {};
    Tex uncheck_icon = {};
    Tex partcheck_icon = {};
    Tex drive_icon = {};
    Tex settings_icon = {};
};

// ============================================================================
// Window/UI State
// ============================================================================

enum WindowState {
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
    WINDOW_STATE_OPENMODE
};

enum SortState {
    FS_SORT_ALPHA_ASC = 0,
    FS_SORT_ALPHA_DESC,
    FS_SORT_SIZE_ASC,
    FS_SORT_SIZE_DESC
};

struct WindowService {
    WindowState state = WINDOW_STATE_FILEBROWSER;
    u64 selected = 0;
    std::vector<FsDirectoryEntry> entries;
    std::vector<FileMetadataCache> metadata_cache;
    s64 used_storage = 0;
    s64 total_storage = 0;
    std::vector<Tex> textures;
    long unsigned int frame_count = 0;
    float zoom_factor = 1.0f;
    float pan_offset_x = 0.0f;
    float pan_offset_y = 0.0f;
    
    // Image viewer pre-loading
    std::vector<Tex> textures_prev;
    std::vector<Tex> textures_next;
    int preload_prev_index = -1;
    int preload_next_index = -1;
    bool image_fullscreen = false;
    
    // Text reader data
    std::string text_content;
    
    // Sort state
    int sort = 0;
};

// ============================================================================
// GUI Service
// ============================================================================

struct GUIService {
    int display_width = 1280;
    int display_height = 720;
    
    // Internal state (managed by GUI module)
    bool is_initialized = false;
};

// ============================================================================
// Application - wires everything together
// ============================================================================

struct App {
    FileSystemService fs;
    DeviceRegistry device_registry;
    ConfigService config;
    SelectionService selection;
    TextureService textures;
    WindowService window;
    GUIService gui;
    
    // Singleton-like access for transition period
    // Will be removed once all code is refactored
    static App* instance;
};

// Global app instance accessor (for transition period)
App& GetApp();
