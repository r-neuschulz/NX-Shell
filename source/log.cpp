#include <cstdarg>

#include "config.hpp"
#include "fs.hpp"
#include "services.hpp"

namespace Log {
    static FsFile file;
    static s64 offset = 0;
    static bool file_is_open = false;
    
    // Internal helper to ensure log file is open (lazy initialization)
    static bool EnsureFileOpen(void) {
        App& app = GetApp();
        if (file_is_open)
            return true;
        
        const char *log_path = "/switch/NX-Shell/debug.log";
        
        if (!FS::FileExists(log_path))
            fsFsCreateFile(std::addressof(app.fs.devices[FileSystemSDMC]), log_path, 0, 0);
            
        if (R_FAILED(fsFsOpenFile(std::addressof(app.fs.devices[FileSystemSDMC]), log_path, (FsOpenMode_Read | FsOpenMode_Write | FsOpenMode_Append), std::addressof(file))))
            return false;
        
        file_is_open = true;
        
        // Seek to end of file for appending
        s64 size = 0;
        if (R_FAILED(fsFileGetSize(std::addressof(file), std::addressof(size))))
            return true; // File is still open, just start at offset 0

        offset = size;
        return true;
    }
    
    void Init(void) {
        App& app = GetApp();
        if (!app.config.effective.dev_options)
            return;
        
        EnsureFileOpen();
    }
    
    void Error(const char *data, ...) {
        App& app = GetApp();
        if (!app.config.effective.dev_options)
            return;
         
        char buf[256 + FS_MAX_PATH];
        va_list args;
        va_start(args, data);
        std::vsnprintf(buf, sizeof(buf), data, args);
        va_end(args);
        
        std::string error_string = "[ERROR] ";
        error_string.append(buf);

        std::printf("%s", error_string.c_str());
        std::fflush(stdout);  // Ensure nxlink output is sent immediately
        
        // Ensure file is open before writing (handles runtime enable of logging)
        if (!EnsureFileOpen())
            return;
        
        // Use FsWriteOption_Flush to ensure write is committed to SD card
        if (R_FAILED(fsFileWrite(std::addressof(file), offset, error_string.data(), error_string.length(), FsWriteOption_Flush)))
            return;

        offset += error_string.length();
    }
    
    void Debug(const char *data, ...) {
        App& app = GetApp();
        if (!app.config.effective.dev_options)
            return;
         
        char buf[256 + FS_MAX_PATH];
        va_list args;
        va_start(args, data);
        std::vsnprintf(buf, sizeof(buf), data, args);
        va_end(args);
        
        std::string debug_string = "[DEBUG] ";
        debug_string.append(buf);

        std::printf("%s", debug_string.c_str());
        std::fflush(stdout);  // Ensure nxlink output is sent immediately
        
        // Ensure file is open before writing (handles runtime enable of logging)
        if (!EnsureFileOpen())
            return;
        
        // Use FsWriteOption_Flush to ensure write is committed to SD card
        if (R_FAILED(fsFileWrite(std::addressof(file), offset, debug_string.data(), debug_string.length(), FsWriteOption_Flush)))
            return;

        offset += debug_string.length();
    }
    
    void Flush(void) {
        App& app = GetApp();
        if (!app.config.effective.dev_options || !file_is_open)
            return;
        
        std::fflush(stdout);  // Flush nxlink
        // File writes with FsWriteOption_Flush are already flushed
    }
    
    void Exit(void) {
        if (!file_is_open)
            return;
        
        fsFileClose(std::addressof(file));
        file_is_open = false;
        offset = 0;
    }
}
