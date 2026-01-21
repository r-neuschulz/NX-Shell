#include <stdio.h>
#include <sys/stat.h>
#include <switch.h>

#include "config.hpp"
#include "fs.hpp"
#include "gui.hpp"
#include "imgui.h"
#include "log.hpp"
#include "net.hpp"
#include "tabs.hpp"
#include "textures.hpp"
#include "windows.hpp"
#include "usb.hpp"

char __application_path[FS_MAX_PATH];

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
        
        // Filesystem setup
        phase_tick = armGetSystemTick();
        devices[FileSystemSDMC] = *fsdevGetDeviceFileSystem("sdmc");
        fs = std::addressof(devices[FileSystemSDMC]);

        // Open BIS filesystems and track success for proper cleanup
        if (R_SUCCEEDED(fsOpenBisFileSystem(std::addressof(devices[FileSystemSafe]), FsBisPartitionId_SafeMode, ""))) {
            fsdevMountDevice("safe", devices[FileSystemSafe]);
            bis_safe_opened = true;
        }

        if (R_SUCCEEDED(fsOpenBisFileSystem(std::addressof(devices[FileSystemUser]), FsBisPartitionId_User, ""))) {
            fsdevMountDevice("user", devices[FileSystemUser]);
            bis_user_opened = true;
        }

        if (R_SUCCEEDED(fsOpenBisFileSystem(std::addressof(devices[FileSystemSystem]), FsBisPartitionId_System, ""))) {
            fsdevMountDevice("system", devices[FileSystemSystem]);
            bis_system_opened = true;
        }
        LogTiming("Filesystem setup", phase_tick);
        
        // Config and logging
        phase_tick = armGetSystemTick();
        Config::Load();
        Log::Init();
        
        // Socket/nxlink only if dev_options enabled (for console output via nxlink)
        // Normal users skip this entirely - saves ~60-70ms
        if (cfg.dev_options) {
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
        if (R_FAILED(ret = USB::Init())) {
            Log::Error("usbHsFsInitialize(0) failed: 0x%x\n", ret);
            return ret;
        }
        LogTiming("USB::Init", phase_tick);

        // GUI and graphics
        phase_tick = armGetSystemTick();
        if (!GUI::Init())
            Log::Error("GUI::Init() failed: 0x%x\n", ret);
        LogTiming("GUI::Init", phase_tick);
        
        // Textures
        phase_tick = armGetSystemTick();
        Textures::Init();
        LogTiming("Textures::Init", phase_tick);
        
        plExit();
        romfsExit();
        return 0;
    }
    
    void Exit(void) {
        // Remove crash marker - we're exiting cleanly
        RemoveCrashMarker();
        
        // Save config before any cleanup to preserve current path
        Config::Save(cfg);
        
        // Clean up textures first (requires valid GL context)
        Textures::Exit();
        
        // Clean up GUI (includes GL context and ImGui)
        GUI::Exit();
        
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
            fsFsClose(std::addressof(devices[FileSystemSystem]));
        }
        if (bis_user_opened) {
            fsdevUnmountDevice("user");
            fsFsClose(std::addressof(devices[FileSystemUser]));
        }
        if (bis_safe_opened) {
            fsdevUnmountDevice("safe");
            fsFsClose(std::addressof(devices[FileSystemSafe]));
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
    
    // Check for crash recovery - if marker exists, previous run crashed
    phase_tick = armGetSystemTick();
    bool previous_crash = CheckCrashMarker();
    if (previous_crash) {
        // Previous run crashed - reset to safe defaults
        Log::Error("Crash marker detected - previous run crashed. Resetting to safe defaults.\n");
        cfg.last_device = "";  // Reset to partition root
        cfg.last_cwd = "/";
        Config::Save(cfg);
    }
    
    // Create crash marker - will be removed on clean exit
    CreateCrashMarker();
    LogTiming("Crash marker check/create", phase_tick);

    // Restore saved path from config, or fall back to partition root if invalid
    phase_tick = armGetSystemTick();
    FS::RestoreSavedPath(data.entries);
    LogTiming("FS::RestoreSavedPath", phase_tick);
    
    // Only populate metadata cache and storage info if we're not at partition root
    phase_tick = armGetSystemTick();
    if (!FS::IsAtPartitionRoot()) {
        FS::PopulateMetadataCache(data.entries, data.metadata_cache);
        FS::GetUsedStorageSpace(data.used_storage);
        FS::GetTotalStorageSpace(data.total_storage);
    } else {
        data.used_storage = 0;
        data.total_storage = 0;
    }
    LogTiming("Metadata cache/storage info", phase_tick);
    
    // Set initial focus (sdmc: at partition root, or ".." in directory)
    Tabs::RequestFileBrowserFocus();
    
    // Log total startup time
    Log::Debug("[TIMING] ========== STARTUP COMPLETE ==========\n");
    LogTiming("Total startup time", s_startup_begin_tick);
    
    while (GUI::Loop(key)) {
        Windows::MainWindow(data, key, false);
        GUI::RenderStatsOverlay();
        GUI::Render();
    }

    data.entries.clear();
    Services::Exit();
    return 0;
}
