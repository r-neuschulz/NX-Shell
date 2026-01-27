#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <switch.h>

#include "config.hpp"
#include "fs.hpp"
#include "gui.hpp"
#include "imgui.h"
#include "log.hpp"
#include "net.hpp"
#include "popups.hpp"
#include "services.hpp"
#include "tabs.hpp"
#include "textures.hpp"
#include "version.hpp"
#include "windows.hpp"
#include "usb.hpp"

char __application_path[FS_MAX_PATH];

// Clean up old NX-Shell NRO files in the same directory as the running app
static void CleanupOldVersions(FsFileSystem *sdmc_fs) {
    // Get directory from application path (e.g., "sdmc:/switch/NX-Shell.nro" -> "/switch/")
    std::string app_path(__application_path);
    
    // Remove "sdmc:" prefix if present
    if (app_path.rfind("sdmc:", 0) == 0) {
        app_path = app_path.substr(5);
    }
    
    // Find directory and current filename
    size_t last_slash = app_path.rfind('/');
    if (last_slash == std::string::npos) {
        return; // No directory separator found
    }
    
    std::string dir_path = app_path.substr(0, last_slash + 1);
    std::string current_filename = app_path.substr(last_slash + 1);
    
    Log::Debug("CleanupOldVersions: dir=%s, current=%s\n", dir_path.c_str(), current_filename.c_str());
    
    // Open directory
    FsDir dir;
    Result ret = fsFsOpenDirectory(sdmc_fs, dir_path.c_str(), FsDirOpenMode_ReadFiles, &dir);
    if (R_FAILED(ret)) {
        Log::Debug("CleanupOldVersions: Failed to open directory: 0x%x\n", ret);
        return;
    }
    
    // Read directory entries
    s64 total_entries = 0;
    FsDirectoryEntry entries[64];
    
    while (true) {
        s64 read_count = 0;
        ret = fsDirRead(&dir, &read_count, 64, entries);
        if (R_FAILED(ret) || read_count == 0) {
            break;
        }
        
        for (s64 i = 0; i < read_count; i++) {
            // Skip directories
            if (entries[i].type == FsDirEntryType_Dir) {
                continue;
            }
            
            std::string filename(entries[i].name);
            
            // Check if it's an NX-Shell NRO file (case-insensitive start, .nro extension)
            if (filename.length() < 12) { // "NX-Shell.nro" is 12 chars minimum
                continue;
            }
            
            // Check for .nro extension (case-insensitive)
            std::string ext = filename.substr(filename.length() - 4);
            if (ext != ".nro" && ext != ".NRO") {
                continue;
            }
            
            // Check if it starts with "NX-Shell" (case-insensitive)
            std::string prefix = filename.substr(0, 8);
            bool is_nxshell = (prefix == "NX-Shell" || prefix == "nx-shell" || prefix == "NX-shell");
            if (!is_nxshell) {
                continue;
            }
            
            // Don't delete the currently running file
            if (filename == current_filename) {
                Log::Debug("CleanupOldVersions: Skipping current file: %s\n", filename.c_str());
                continue;
            }
            
            // Delete old NX-Shell NRO
            std::string full_path = dir_path + filename;
            Log::Debug("CleanupOldVersions: Deleting old version: %s\n", full_path.c_str());
            
            ret = fsFsDeleteFile(sdmc_fs, full_path.c_str());
            if (R_SUCCEEDED(ret)) {
                total_entries++;
                Log::Debug("CleanupOldVersions: Successfully deleted: %s\n", full_path.c_str());
            } else {
                Log::Debug("CleanupOldVersions: Failed to delete %s: 0x%x\n", full_path.c_str(), ret);
            }
        }
    }
    
    fsDirClose(&dir);
    
    if (total_entries > 0) {
        Log::Debug("CleanupOldVersions: Cleaned up %lld old NRO file(s)\n", total_entries);
    }
}

// Track which BIS filesystems were successfully opened
static bool bis_safe_opened = false;
static bool bis_user_opened = false;
static bool bis_system_opened = false;

// Crash recovery marker file path
static const char *CRASH_MARKER_PATH = "/switch/NX-Shell/.crash_marker";

// Check if a previous crash occurred (marker file exists)
static bool CheckCrashMarker(void) {
    struct stat st;
    return (stat(CRASH_MARKER_PATH, &st) == 0);
}

// Create crash marker at startup
static void CreateCrashMarker(void) {
    FILE *f = fopen(CRASH_MARKER_PATH, "w");
    if (f) {
        fprintf(f, "crash_marker");
        fclose(f);
    }
}

// Remove crash marker on clean exit
static void RemoveCrashMarker(void) {
    remove(CRASH_MARKER_PATH);
}

// Startup timing helpers
static u64 s_startup_begin_tick = 0;

// Convert system ticks to milliseconds
static float TicksToMs(u64 ticks) {
    // ARM tick frequency: 19.2 MHz (19200000 ticks per second)
    // Formula: ticks * 1000 / 19200000 = ticks / 19200
    return static_cast<float>(ticks) / 19200.0f;
}

// Log elapsed time since a start tick
static void LogTiming(const char *label, u64 start_tick) {
    u64 elapsed = armGetSystemTick() - start_tick;
    Log::Debug("[TIMING] %s: %.2f ms\n", label, TicksToMs(elapsed));
}

namespace Services {
    // Set the application path from argv (called from main before Init)
    void SetApplicationPath(int argc, char* argv[]) {
        if (argc > 0 && argv[0] != nullptr) {
            strncpy(__application_path, argv[0], FS_MAX_PATH - 1);
            __application_path[FS_MAX_PATH - 1] = '\0';
        } else {
            __application_path[0] = '\0';
        }
    }
    
    int Init(void) {
        Result ret = 0;
        u64 phase_tick;
        
        // Get App instance for initialization
        App& app = GetApp();
        
        // Filesystem setup
        phase_tick = armGetSystemTick();
        app.fs.devices[FileSystemSDMC] = *fsdevGetDeviceFileSystem("sdmc");
        app.fs.current_fs = std::addressof(app.fs.devices[FileSystemSDMC]);

        // Open BIS filesystems and track success for proper cleanup
        if (R_SUCCEEDED(fsOpenBisFileSystem(std::addressof(app.fs.devices[FileSystemSafe]), FsBisPartitionId_SafeMode, ""))) {
            fsdevMountDevice("safe", app.fs.devices[FileSystemSafe]);
            bis_safe_opened = true;
        }

        if (R_SUCCEEDED(fsOpenBisFileSystem(std::addressof(app.fs.devices[FileSystemUser]), FsBisPartitionId_User, ""))) {
            fsdevMountDevice("user", app.fs.devices[FileSystemUser]);
            bis_user_opened = true;
        }

        if (R_SUCCEEDED(fsOpenBisFileSystem(std::addressof(app.fs.devices[FileSystemSystem]), FsBisPartitionId_System, ""))) {
            fsdevMountDevice("system", app.fs.devices[FileSystemSystem]);
            bis_system_opened = true;
        }
        LogTiming("Filesystem setup", phase_tick);
        
        // Config and logging
        phase_tick = armGetSystemTick();
        Config::Load(app.config, app.fs, GUI::IsAppletMode());
        Log::Init(app);
        
        // Socket/nxlink only if logging enabled (for console output via nxlink)
        // Normal users skip this entirely - saves ~60-70ms
        if (app.config.DevOptions()) {
            Net::InitSocketWithNxlink();
        }
        LogTiming("Config/Log/Socket init", phase_tick);

        // ROM filesystem
        phase_tick = armGetSystemTick();
        if (R_FAILED(ret = romfsInit())) {
            Log::Error("romfsInit() failed: 0x%x\n", ret);
            return ret;
        }
        LogTiming("romfsInit", phase_tick);
        
        // Network interface
        phase_tick = armGetSystemTick();
        if (R_FAILED(ret = nifmInitialize(NifmServiceType_User))) {
            Log::Error("nifmInitialize(NifmServiceType_User) failed: 0x%x\n", ret);
            return ret;
        }
        LogTiming("nifmInitialize", phase_tick);
        
        // Font service
        phase_tick = armGetSystemTick();
        if (R_FAILED(ret = plInitialize(PlServiceType_User))) {
            Log::Error("plInitialize(PlServiceType_User) failed: 0x%x\n", ret);
            return ret;
        }
        LogTiming("plInitialize", phase_tick);

        // Settings services
        phase_tick = armGetSystemTick();
        if (R_FAILED(ret = setInitialize())) {
            Log::Error("setInitialize() failed: 0x%x\n", ret);
            return ret;
        }

        if (R_FAILED(ret = setsysInitialize())) {
            Log::Error("setsysInitialize() failed: 0x%x\n", ret);
            // Non-fatal: continue without system settings (auto theme will default to dark)
        }
        LogTiming("Settings services", phase_tick);

        // Temperature services
        phase_tick = armGetSystemTick();
        if (R_FAILED(ret = tsInitialize())) {
            Log::Error("tsInitialize() failed: 0x%x\n", ret);
            // Non-fatal: continue without temperature readings
        }

        if (R_FAILED(ret = tcInitialize())) {
            Log::Error("tcInitialize() failed: 0x%x\n", ret);
            // Non-fatal: continue without skin temperature readings
        }
        LogTiming("Temperature services", phase_tick);

        // Clock rate service
        phase_tick = armGetSystemTick();
        if (R_FAILED(ret = clkrstInitialize())) {
            Log::Error("clkrstInitialize() failed: 0x%x\n", ret);
            // Non-fatal: continue without clock rate readings
        }
        LogTiming("clkrstInitialize", phase_tick);

        // Power/battery service
        phase_tick = armGetSystemTick();
        if (R_FAILED(ret = psmInitialize())) {
            Log::Error("psmInitialize() failed: 0x%x\n", ret);
            // Non-fatal: continue without battery readings
        }
        LogTiming("psmInitialize", phase_tick);

        // WiFi info service
        phase_tick = armGetSystemTick();
        if (R_FAILED(ret = wlaninfInitialize())) {
            Log::Error("wlaninfInitialize() failed: 0x%x\n", ret);
            // Non-fatal: continue without WiFi signal strength
        }
        LogTiming("wlaninfInitialize", phase_tick);

        // USB host filesystem
        phase_tick = armGetSystemTick();
        if (R_FAILED(ret = USB::Init(app.device_registry))) {
            Log::Error("usbHsFsInitialize(0) failed: 0x%x\n", ret);
            return ret;
        }
        LogTiming("USB::Init", phase_tick);

        // GUI and graphics
        phase_tick = armGetSystemTick();
        if (!GUI::Init(app))
            Log::Error("GUI::Init() failed: 0x%x\n", ret);
        LogTiming("GUI::Init", phase_tick);
        
        // Textures
        phase_tick = armGetSystemTick();
        Textures::Init(app);
        LogTiming("Textures::Init", phase_tick);
        
        plExit();
        romfsExit();
        return 0;
    }
    
    void Exit(void) {
        // Remove crash marker - we're exiting cleanly
        RemoveCrashMarker();
        
        // Get App instance for cleanup
        App& app = GetApp();
        
        // Save config before any cleanup to preserve current path
        Config::Save(app.config, app.fs);
        
        // Clean up textures first (requires valid GL context)
        Textures::Exit(app);
        
        // Clean up GUI (includes GL context and ImGui)
        GUI::Exit(app);
        
        // Stop USB thread and cleanup (do this early to avoid hangs)
        USB::Exit();
        
        // Exit services in reverse order of initialization
        wlaninfExit();
        psmExit();
        clkrstExit();
        tcExit();
        tsExit();
        setsysExit();
        setExit();
        nifmExit();
        
        // Close socket last among services (only if it was initialized)
        Net::ExitSocket();
        
        // Close log file
        Log::Exit();
        
        // Unmount and close BIS filesystems (only those that were successfully opened)
        if (bis_system_opened) {
            fsdevUnmountDevice("system");
            fsFsClose(std::addressof(app.fs.devices[FileSystemSystem]));
        }
        if (bis_user_opened) {
            fsdevUnmountDevice("user");
            fsFsClose(std::addressof(app.fs.devices[FileSystemUser]));
        }
        if (bis_safe_opened) {
            fsdevUnmountDevice("safe");
            fsFsClose(std::addressof(app.fs.devices[FileSystemSafe]));
        }
    }
}

int main(int argc, char* argv[]) {
    u64 key = 0;
    u64 phase_tick;
    
    // Record startup begin time
    s_startup_begin_tick = armGetSystemTick();
    Log::Debug("[TIMING] ========== STARTUP BEGIN ==========\n");

    // Set application path from argv (homebrew launcher passes NRO path in argv[0])
    Services::SetApplicationPath(argc, argv);
    
    phase_tick = armGetSystemTick();
    Services::Init();
    LogTiming("Services::Init (total)", phase_tick);
    
    // Get App instance
    App& app = GetApp();
    
    // Clean up old NX-Shell versions in the same directory (e.g., after update)
    phase_tick = armGetSystemTick();
    CleanupOldVersions(&app.fs.devices[FileSystemSDMC]);
    LogTiming("CleanupOldVersions", phase_tick);
    
    // Check for crash recovery - if marker exists, previous run crashed
    phase_tick = armGetSystemTick();
    bool previous_crash = CheckCrashMarker();
    if (previous_crash) {
        // Previous run crashed - reset to safe defaults
        Log::Error("Crash marker detected - previous run crashed. Resetting to safe defaults.\n");
        app.config.SetLastDevice("");  // Reset to partition root
        app.config.SetLastCwd("/");
        Config::Save(app.config, app.fs);
    }
    
    // Create crash marker - will be removed on clean exit
    CreateCrashMarker();
    LogTiming("Crash marker check/create", phase_tick);

    // Restore saved path from config, or fall back to partition root if invalid
    phase_tick = armGetSystemTick();
    FS::RestoreSavedPath(app.fs, app.device_registry, app.config, app.window.entries);
    LogTiming("FS::RestoreSavedPath", phase_tick);
    
    // Only populate metadata cache and storage info if we're not at partition root
    phase_tick = armGetSystemTick();
    if (!FS::IsAtPartitionRoot(app.fs)) {
        FS::PopulateMetadataCache(app.fs, app.window.entries, app.window.metadata_cache);
        FS::GetUsedStorageSpace(app.fs, app.window.used_storage);
        FS::GetTotalStorageSpace(app.fs, app.window.total_storage);
    } else {
        app.window.used_storage = 0;
        app.window.total_storage = 0;
    }
    LogTiming("Metadata cache/storage info", phase_tick);
    
    // Set initial focus (sdmc: at partition root, or ".." in directory)
    Tabs::RequestFileBrowserFocus(app.fs);
    
    // Use compile-time version string for comparison and welcome popup
    const char* current_version = NX_SHELL_VERSION_STR;
    
    // Check if this is a new version compared to last known
    const std::string& last_known = app.config.LastKnownVersion();
    bool show_welcome_popup = false;
    
    if (last_known.empty()) {
        // First run ever - just save current version, no popup
        Log::Debug("First run detected, saving version %s to config\n", current_version);
        app.config.SetLastKnownVersion(current_version);
        Config::Save(app.config, app.fs);
    } else if (last_known != current_version) {
        // Version changed - show welcome popup
        Log::Debug("Version changed: %s -> %s, will show welcome popup\n", last_known.c_str(), current_version);
        show_welcome_popup = true;
    }
    
    // Log total startup time
    Log::Debug("[TIMING] ========== STARTUP COMPLETE ==========\n");
    LogTiming("Total startup time", s_startup_begin_tick);
    
    while (GUI::Loop(app, key)) {
        Windows::MainWindow(app, key, false);
        
        // Show welcome popup on version change (update from any source)
        if (show_welcome_popup) {
            Popups::UpdateWelcomePopup(app, show_welcome_popup, current_version);
            
            // When popup closes, save new version to config
            if (!show_welcome_popup) {
                app.config.SetLastKnownVersion(current_version);
                Config::Save(app.config, app.fs);
                Log::Debug("Welcome popup closed, saved version %s to config\n", current_version);
            }
        }
        
        GUI::RenderStatsOverlay(app);
        Toast::RenderTimed(app);
        GUI::Render();
    }

    app.window.entries.clear();
    Services::Exit();
    return 0;
}
