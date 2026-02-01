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
    FileTypeBinary,
    FileTypeSwitch,
    FileTypeSwitchInstallable  // NSP files that can be installed
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

// Normal mode: full configuration with all user-customizable settings
struct NormalConfig {
    int lang = LANG_AUTO;
    int resolved_lang = 1;  // Computed from lang at load time (not persisted)
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
    std::string last_known_version;  // Tracks last version user ran; used to show welcome popup on updates
};

// Applet mode: limited settings (most values are forced defaults)
struct AppletConfig {
    bool dev_options = false;
    std::string last_device = "sdmc:";
    std::string last_cwd = "/";
};

// Unified config service - use accessors to get effective values
struct ConfigService {
    NormalConfig normal;   // Persisted normal-mode settings
    AppletConfig applet;   // Persisted applet-mode settings
    bool is_applet_mode = false;
    
    // Effective value accessors - these return the correct value based on mode
    // Applet mode forces most settings to safe defaults
    int Lang() const { 
        return is_applet_mode ? 1 : normal.resolved_lang; 
    }
    bool DevOptions() const { 
        return is_applet_mode ? applet.dev_options : normal.dev_options; 
    }
    bool ImageFilename() const { 
        return is_applet_mode ? false : normal.image_filename; 
    }
    bool EnterImagesFullscreen() const { 
        return is_applet_mode ? false : normal.enter_images_fullscreen; 
    }
    int ResolutionMode() const { 
        return is_applet_mode ? ResolutionMode_720p : normal.resolution_mode; 
    }
    int ThemeMode() const { 
        return is_applet_mode ? ThemeMode_Dark : normal.theme_mode; 
    }
    bool ShowDetails() const { 
        return is_applet_mode ? false : normal.show_details; 
    }
    bool ShowStats() const { 
        return is_applet_mode ? false : normal.show_stats; 
    }
    const std::string& LastDevice() const { 
        return is_applet_mode ? applet.last_device : normal.last_device; 
    }
    const std::string& LastCwd() const { 
        return is_applet_mode ? applet.last_cwd : normal.last_cwd; 
    }
    float AccentR() const { return is_applet_mode ? 0.0f : normal.accent_color[0]; }
    float AccentG() const { return is_applet_mode ? 0.50f : normal.accent_color[1]; }
    float AccentB() const { return is_applet_mode ? 0.50f : normal.accent_color[2]; }
    int ButtonStyle() const { 
        return is_applet_mode ? ButtonStyle_Mono : normal.button_style; 
    }
    
    // Mutable accessors for settings UI (always modifies normal config)
    // Show details can be toggled in both modes for the current session
    void SetShowDetails(bool val) {
        if (is_applet_mode) return;  // Not persisted in applet mode
        normal.show_details = val;
    }
    
    // Navigation state updates the appropriate config
    void SetLastDevice(const std::string& dev) {
        if (is_applet_mode) applet.last_device = dev;
        else normal.last_device = dev;
    }
    void SetLastCwd(const std::string& cwd) {
        if (is_applet_mode) applet.last_cwd = cwd;
        else normal.last_cwd = cwd;
    }
    
    // Version tracking (for post-update welcome popup)
    const std::string& LastKnownVersion() const { return normal.last_known_version; }
    void SetLastKnownVersion(const std::string& version) { normal.last_known_version = version; }
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
    WINDOW_STATE_OPENMODE,
    WINDOW_STATE_NSP_INSTALL,
    WINDOW_STATE_NRO_FORWARDER
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
    bool request_exit = false;
    
    static App* instance;
};

// Global app instance accessor - use only at program entry points (main, init, exit)
// All other code should receive App& as a parameter for explicit dependency injection
App& GetApp();
