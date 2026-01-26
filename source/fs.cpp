#include <algorithm>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <filesystem>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "config.hpp"
#include "fs.hpp"
#include "gui.hpp"
#include "language.hpp"
#include "log.hpp"
#include "popups.hpp"
#include "services.hpp"

// Legacy globals defined in legacy.cpp

namespace FS {

    // ========================================================================
    // Static helpers
    // ========================================================================
    
    static bool ChangeDir(FileSystemService &fs_svc, ConfigService &config_svc, const std::string &path, std::vector<FsDirectoryEntry> &entries) {
        std::vector<FsDirectoryEntry> new_entries;
        const std::string new_path = path;
        fs_svc.cwd = path;
        
        bool ret = FS::GetDirList(fs_svc.device, new_path, new_entries);
        
        if (ret) {
            SaveCurrentPath(fs_svc, config_svc);
        }
        
        entries.clear();
        entries = new_entries;
        return ret;
    }

    static bool CopyFile(const std::string &src_path, const std::string &dest_path) {
        FILE *src = fopen(src_path.c_str(), "rb");
        if (!src) {
            Log::Error("FS::CopyFile (%s) failed to open src file.\n", src_path.c_str());
            return false;
        }

        struct stat file_stat = { 0 };
        if (stat(src_path.c_str(), std::addressof(file_stat)) != 0) {
            Log::Error("FS::CopyFile (%s) failed to get src file size.\n", src_path.c_str());
            return false;
        }

        std::size_t size = file_stat.st_size;

        FILE *dest = fopen(dest_path.c_str(), "wb");
        if (!dest) {
            Log::Error("FS::CopyFile (%s) failed to open dest file.\n", dest_path.c_str());
            fclose(src);
            return false;
        }

        std::size_t bytes_read = 0, offset = 0;
        const std::size_t buf_size = 0x10000;
        unsigned char *buf = new unsigned char[buf_size];
        std::string filename = std::filesystem::path(src_path).filename();

        do {
            std::memset(buf, 0, buf_size);

            bytes_read = fread(buf, sizeof(unsigned char), buf_size, src);
            if (bytes_read < 0) {
                Log::Error("FS::CopyFile (%s) failed to read src file.\n", src_path.c_str());
                delete[] buf;
                fclose(src);
                fclose(dest);
                return false;
            }
            
            std::size_t bytes_written = fwrite(buf, sizeof(unsigned char), bytes_read, dest);
            if (bytes_written != bytes_read) {
                Log::Error("FS::CopyFile (%s) failed to write to dest file.\n", dest_path.c_str());
                delete[] buf;
                fclose(src);
                fclose(dest);
                return false;
            }
            
            offset += bytes_read;
            Popups::ProgressBar(static_cast<float>(offset), static_cast<float>(size), strings[Config::GetLang()][Lang::OptionsCopying], filename.c_str());
        } while (offset < size);

        delete[] buf;
        fclose(src);
        fclose(dest);
        return true;
    }

    static bool CopyDir(const std::string &src_path, const std::string &dest_path) {
        DIR *dir = nullptr;
        struct dirent *entry = nullptr;
        dir = opendir(src_path.c_str());

        if (dir) {
            mkdir(dest_path.c_str(), 0700);

            while((entry = readdir(dir))) {
                std::string filename = entry->d_name;
                if ((filename.compare(".") == 0) || (filename.compare("..") == 0))
                    continue;

                std::string src = src_path;
                src.append("/");
                src.append(filename);

                std::string dest = dest_path;
                dest.append("/");
                dest.append(filename);

                bool copy_result = false;
                if (entry->d_type & DT_DIR)
                    copy_result = CopyDir(src.c_str(), dest.c_str());
                else
                    copy_result = CopyFile(src.c_str(), dest.c_str());
                
                if (!copy_result) {
                    closedir(dir);
                    return false;
                }
            }

            closedir(dir);
        }
        else {
            Log::Error("FS::CopyDir(%s) failed to open path.\n", src_path.c_str());
            return false;
        }

        return true;
    }

    // File extension to FileType lookup table
    static const std::unordered_map<std::string, FileType> extension_map = {
        // Archive formats
        {".ZIP", FileTypeArchive}, {".RAR", FileTypeArchive}, {".7Z", FileTypeArchive},
        
        // Image formats
        {".BMP", FileTypeImage}, {".GIF", FileTypeImage}, {".JPG", FileTypeImage},
        {".JPEG", FileTypeImage}, {".PGM", FileTypeImage}, {".PPM", FileTypeImage},
        {".PNG", FileTypeImage}, {".PSD", FileTypeImage}, {".TGA", FileTypeImage},
        {".WEBP", FileTypeImage},
        
        // Binary/Executable formats
        {".BIN", FileTypeBinary}, {".DAT", FileTypeBinary}, {".ROM", FileTypeBinary},
        {".NRO", FileTypeBinary}, {".NSO", FileTypeBinary}, {".NCA", FileTypeBinary},
        {".NSP", FileTypeBinary}, {".XCI", FileTypeBinary},
        {".EXE", FileTypeBinary}, {".DLL", FileTypeBinary}, {".SYS", FileTypeBinary},
        {".SO", FileTypeBinary}, {".DYLIB", FileTypeBinary}, {".A", FileTypeBinary},
        {".O", FileTypeBinary}, {".ELF", FileTypeBinary}, {".AXF", FileTypeBinary},
        {".FW", FileTypeBinary}, {".BIOS", FileTypeBinary},
        
        // Text formats - common
        {".TXT", FileTypeText}, {".LOG", FileTypeText}, {".MD", FileTypeText},
        {".MARKDOWN", FileTypeText},
        
        // Text formats - configuration
        {".JSON", FileTypeText}, {".XML", FileTypeText}, {".YAML", FileTypeText},
        {".YML", FileTypeText}, {".CFG", FileTypeText}, {".INI", FileTypeText},
        {".CONF", FileTypeText}, {".CONFIG", FileTypeText}, {".TOML", FileTypeText},
        {".ENV", FileTypeText}, {".PROPERTIES", FileTypeText},
        
        // Text formats - C/C++
        {".C", FileTypeText}, {".CPP", FileTypeText}, {".CC", FileTypeText},
        {".CXX", FileTypeText}, {".H", FileTypeText}, {".HPP", FileTypeText},
        {".HH", FileTypeText}, {".HXX", FileTypeText},
        
        // Text formats - JVM languages
        {".CS", FileTypeText}, {".JAVA", FileTypeText}, {".KT", FileTypeText},
        {".SCALA", FileTypeText},
        
        // Text formats - Python
        {".PY", FileTypeText}, {".PYW", FileTypeText}, {".PYX", FileTypeText},
        
        // Text formats - Web
        {".JS", FileTypeText}, {".JSX", FileTypeText}, {".TS", FileTypeText},
        {".TSX", FileTypeText}, {".HTML", FileTypeText}, {".HTM", FileTypeText},
        {".CSS", FileTypeText}, {".SCSS", FileTypeText}, {".SASS", FileTypeText},
        {".LESS", FileTypeText}, {".PHP", FileTypeText},
        
        // Text formats - Ruby
        {".RB", FileTypeText}, {".RUBY", FileTypeText},
        
        // Text formats - Other languages
        {".GO", FileTypeText}, {".RS", FileTypeText}, {".RUST", FileTypeText},
        {".SWIFT", FileTypeText}, {".M", FileTypeText}, {".MM", FileTypeText},
        {".LUA", FileTypeText}, {".PL", FileTypeText}, {".PM", FileTypeText},
        {".PERL", FileTypeText},
        
        // Text formats - Shell scripts
        {".SH", FileTypeText}, {".BASH", FileTypeText}, {".ZSH", FileTypeText},
        {".FISH", FileTypeText}, {".BAT", FileTypeText}, {".CMD", FileTypeText},
        {".PS1", FileTypeText},
        
        // Text formats - Data/Query
        {".SQL", FileTypeText}, {".R", FileTypeText}, {".MATLAB", FileTypeText},
        {".OCTAVE", FileTypeText}, {".CSV", FileTypeText}, {".TSV", FileTypeText},
        
        // Text formats - Assembly
        {".ASM", FileTypeText}, {".S", FileTypeText},
        
        // Text formats - Documentation
        {".RST", FileTypeText}, {".TEX", FileTypeText}, {".LATEX", FileTypeText},
        {".NFO", FileTypeText}, {".DIZ", FileTypeText},
        
        // Text formats - Build/Project files
        {".CMAKE", FileTypeText}, {".MAKEFILE", FileTypeText}, {".MAKE", FileTypeText},
        {".GRADLE", FileTypeText}, {".MAVEN", FileTypeText}, {".SBT", FileTypeText},
        {".GITIGNORE", FileTypeText}, {".GITATTRIBUTES", FileTypeText},
        {".GITMODULES", FileTypeText}, {".DOCKERIGNORE", FileTypeText},
        {".EDITORCONFIG", FileTypeText},
        
        // Text formats - Nintendo Switch specific
        {".PCHTXT", FileTypeText}, {".IPS", FileTypeText},
    };
    
    // Common extensionless text files (case-insensitive basename match)
    static const std::unordered_set<std::string> text_basenames = {
        "README", "LICENSE", "LICENCE", "CHANGELOG", "AUTHORS", "CONTRIBUTORS",
        "MAKEFILE", "DOCKERFILE", "CMAKELISTS.TXT", "GEMFILE", "RAKEFILE", "VAGRANTFILE"
    };

    // ========================================================================
    // New API - takes service references
    // ========================================================================

    bool DestinationExists(FileSystemService &fs_svc) {
        if (fs_svc.copy_entry.filename.empty())
            return false;
        
        std::string dest_path = BuildPath(fs_svc, fs_svc.copy_entry.filename, true);
        struct stat dest_stat = { 0 };
        return (stat(dest_path.c_str(), std::addressof(dest_stat)) == 0);
    }

    size_t CountConflicts(FileSystemService &fs_svc, SelectionService &sel_svc) {
        const auto &selected_paths = sel_svc.selected_paths;
        size_t conflicts = 0;
        
        for (const auto &full_path : selected_paths) {
            size_t last_slash = full_path.rfind('/');
            std::string filename = (last_slash != std::string::npos) ? full_path.substr(last_slash + 1) : full_path;
            if (filename == "..")
                continue;
            
            std::string dest_path = BuildPath(fs_svc, filename, true);
            struct stat dest_stat = { 0 };
            if (stat(dest_path.c_str(), &dest_stat) == 0) {
                conflicts++;
            }
        }
        
        return conflicts;
    }
    
    void SetConflictHandling(FileSystemService &fs_svc, ConflictHandling mode) {
        fs_svc.conflict_mode = mode;
    }
    
    ConflictHandling GetConflictHandling(FileSystemService &fs_svc) {
        return fs_svc.conflict_mode;
    }
    
    void ClearConflictHandling(FileSystemService &fs_svc) {
        fs_svc.conflict_mode = ConflictHandling_Ask;
    }
    
    bool ShouldSkipDueToConflict(FileSystemService &fs_svc, const std::string &dest_path) {
        if (fs_svc.conflict_mode != ConflictHandling_SkipAll)
            return false;
        
        struct stat dest_stat = { 0 };
        return (stat(dest_path.c_str(), &dest_stat) == 0);
    }

    bool ChangeDirNext(FileSystemService &fs_svc, const std::string &path, std::vector<FsDirectoryEntry> &entries) {
        App& app = GetApp();
        return ChangeDir(fs_svc, app.config, BuildPath(fs_svc, path, false), entries);
    }
    
    bool ChangeDirPrev(FileSystemService &fs_svc, std::vector<FsDirectoryEntry> &entries) {
        App& app = GetApp();
        if (fs_svc.cwd.compare("/") == 0)
            return false;
        
        std::filesystem::path path = fs_svc.cwd;
        std::string parent_path = path.parent_path();
        return ChangeDir(fs_svc, app.config, parent_path.empty() ? fs_svc.cwd : parent_path, entries);
    }

    bool Rename(FileSystemService &fs_svc, FsDirectoryEntry &entry, const std::string &dest_path) {
        std::string src_path = BuildPath(fs_svc, entry);
        std::string full_dest_path = BuildPath(fs_svc, dest_path, true);

        if (rename(src_path.c_str(), full_dest_path.c_str()) != 0) {
            Log::Error("FS::Rename(%s, %s) failed.\n", src_path.c_str(), dest_path.c_str());
            return false;
        }
        
        return true;
    }

    bool Delete(FileSystemService &fs_svc, FsDirectoryEntry &entry) {
        std::string full_path = BuildPath(fs_svc, entry);

        if (entry.type == FsDirEntryType_Dir) {
            if (!DeleteRecursive(full_path)) {
                Log::Error("FS::Delete(%s) failed to delete folder.\n", full_path.c_str());
                return false;
            }
        }
        else {
            if (remove(full_path.c_str()) != 0) {
                Log::Error("FS::Delete(%s) failed to delete file.\n", full_path.c_str());
                return false;
            }
        }
        
        return true;
    }

    void Copy(FileSystemService &fs_svc, FsDirectoryEntry &entry, const std::string &path) {
        std::string full_path = path;
        full_path.append(path.compare("/") == 0 ? "" : "/");
        full_path.append(entry.name);
        
        if ((std::strncmp(entry.name, "..", 2)) != 0) {
            fs_svc.copy_entry.path = full_path;
            fs_svc.copy_entry.filename = entry.name;
            
            if (entry.type == FsDirEntryType_Dir)
                fs_svc.copy_entry.is_directory = true;
        }
    }

    bool WouldCauseRecursiveCopy(FileSystemService &fs_svc) {
        if (!fs_svc.copy_entry.is_directory)
            return false;
        
        std::string src_path = fs_svc.copy_entry.path;
        std::string dest_parent = fs_svc.device + fs_svc.cwd;
        
        if (!src_path.empty() && src_path.back() == '/')
            src_path.pop_back();
        if (!dest_parent.empty() && dest_parent.back() == '/')
            dest_parent.pop_back();
        
        std::string src_with_slash = src_path + "/";
        if (dest_parent.compare(0, src_with_slash.length(), src_with_slash) == 0) {
            return true;
        }
        
        return false;
    }

    bool Paste(FileSystemService &fs_svc, ConfigService &config_svc) {
        bool ret = false;
        std::string path = BuildPath(fs_svc, fs_svc.copy_entry.filename, true);
        
        if (fs_svc.copy_entry.is_directory)
            ret = CopyDir(fs_svc.copy_entry.path, path);
        else
            ret = CopyFile(fs_svc.copy_entry.path, path);

        fs_svc.copy_entry = {};
        return ret;
    }

    bool Move(FileSystemService &fs_svc, ConfigService &config_svc) {
        std::string path = BuildPath(fs_svc, fs_svc.copy_entry.filename, true);

        if (rename(fs_svc.copy_entry.path.c_str(), path.c_str()) != 0) {
            Log::Error("FS::Move(%s, %s) failed.\n", fs_svc.copy_entry.path.c_str(), path.c_str());
            return false;
        }

        fs_svc.copy_entry = {};
        return true;
    }

    std::string GetCopyEntryFilename(FileSystemService &fs_svc) {
        return fs_svc.copy_entry.filename;
    }
    
    Result SetArchiveBit(FileSystemService &fs_svc, const std::string &path) {
        Result ret = 0;

        char fs_path[FS_MAX_PATH];
        std::snprintf(fs_path, FS_MAX_PATH, path.c_str());

        if (R_FAILED(ret = fsFsSetConcatenationFileAttribute(&fs_svc.devices[FileSystemSDMC], fs_path))) {
            Log::Error("fsFsSetConcatenationFileAttribute(%s) failed: 0x%x\n", path.c_str(), ret);
            return ret;
        }
        
        return 0;
    }
    
    bool HasArchiveBit(FileSystemService &fs_svc, const std::string &path) {
        char fs_path[FS_MAX_PATH];
        std::snprintf(fs_path, FS_MAX_PATH, "%s", path.c_str());
        
        FsFileSystem *target_fs = nullptr;
        const char *rel_path = fs_path;
        
        if (std::strncmp(fs_path, "sdmc:", 5) == 0) {
            target_fs = &fs_svc.devices[FileSystemSDMC];
            rel_path = fs_path + 5;
        }
        else if (std::strncmp(fs_path, "safe:", 5) == 0) {
            target_fs = &fs_svc.devices[FileSystemSafe];
            rel_path = fs_path + 5;
        }
        else if (std::strncmp(fs_path, "user:", 5) == 0) {
            target_fs = &fs_svc.devices[FileSystemUser];
            rel_path = fs_path + 5;
        }
        else if (std::strncmp(fs_path, "system:", 7) == 0) {
            target_fs = &fs_svc.devices[FileSystemSystem];
            rel_path = fs_path + 7;
        }
        else {
            target_fs = fs_svc.current_fs;
        }
        
        if (target_fs == nullptr) {
            return false;
        }
        
        FsDirEntryType entry_type;
        Result ret = fsFsGetEntryType(target_fs, rel_path, &entry_type);
        if (R_FAILED(ret))
            return false;
        
        struct stat file_stat = { 0 };
        if (stat(path.c_str(), &file_stat) != 0)
            return false;
        
        if (S_ISDIR(file_stat.st_mode) && entry_type == FsDirEntryType_File)
            return true;
        
        return false;
    }
    
    Result GetFreeStorageSpace(FileSystemService &fs_svc, s64 &size) {
        Result ret = 0;
        
        if (R_FAILED(ret = fsFsGetFreeSpace(fs_svc.current_fs, "/", std::addressof(size)))) {
            Log::Error("fsFsGetFreeSpace() failed: 0x%x\n", ret);
            return ret;
        }
        
        return 0;
    }
    
    Result GetTotalStorageSpace(FileSystemService &fs_svc, s64 &size) {
        Result ret = 0;
        
        if (R_FAILED(ret = fsFsGetTotalSpace(fs_svc.current_fs, "/", std::addressof(size)))) {
            Log::Error("fsFsGetTotalSpace() failed: 0x%x\n", ret);
            return ret;
        }
        
        return 0;
    }
    
    Result GetUsedStorageSpace(FileSystemService &fs_svc, s64 &size) {
        Result ret = 0;
        s64 free_size = 0, total_size = 0;
        
        if (R_FAILED(ret = GetFreeStorageSpace(fs_svc, free_size)))
            return ret;
            
        if (R_FAILED(ret = GetTotalStorageSpace(fs_svc, total_size)))
            return ret;
        
        size = (total_size - free_size);
        return 0;
    }

    std::string BuildPath(FileSystemService &fs_svc, FsDirectoryEntry &entry) {
        std::string path_next = fs_svc.device;
        path_next.append(fs_svc.cwd);
        path_next.append((fs_svc.cwd.compare("/") == 0) ? "" : "/");
        path_next.append(entry.name);
        return path_next;
    }

    std::string BuildPath(FileSystemService &fs_svc, const std::string &path, bool device_name) {
        std::string path_next = "";

        if (device_name)
            path_next.append(fs_svc.device);
        
        path_next.append(fs_svc.cwd);
        path_next.append((fs_svc.cwd.compare("/") == 0) ? "" : "/");
        path_next.append(path);
        return path_next;
    }

    void PopulateMetadataCache(FileSystemService &fs_svc, const std::vector<FsDirectoryEntry> &entries, std::vector<FileMetadataCache> &cache) {
        cache.clear();
        
        FileMetadataCache empty_cache;
        empty_cache.file_size = 0;
        empty_cache.modified_time = 0;
        empty_cache.has_archive_bit = false;
        empty_cache.valid = false;
        cache.assign(entries.size(), empty_cache);
        
        for (size_t i = 0; i < entries.size(); i++) {
            if (std::strncmp(entries[i].name, "..", 2) == 0) {
                continue;
            }
            
            std::string full_path = BuildPath(fs_svc, const_cast<FsDirectoryEntry&>(entries[i]));
            struct stat file_stat = { 0 };
            
            if (stat(full_path.c_str(), std::addressof(file_stat)) == 0) {
                cache[i].file_size = file_stat.st_size;
                cache[i].modified_time = file_stat.st_mtime;
                cache[i].valid = true;
                
                if (entries[i].type == FsDirEntryType_Dir) {
                    cache[i].has_archive_bit = HasArchiveBit(fs_svc, full_path);
                }
            }
        }
    }
    
    bool RefreshDirectory(FileSystemService &fs_svc, SelectionService &sel_svc, std::vector<FsDirectoryEntry> &entries, std::vector<FileMetadataCache> &cache, bool clear_selection) {
        if (!GetDirList(fs_svc.device, fs_svc.cwd, entries)) {
            Log::Error("FS::RefreshDirectory() failed to get directory list.\n");
            return false;
        }
        PopulateMetadataCache(fs_svc, entries, cache);
        if (clear_selection)
            sel_svc.Clear();
        return true;
    }
    
    bool IsAtPartitionRoot(FileSystemService &fs_svc) {
        return fs_svc.device.empty();
    }
    
    void GetPartitionList(DeviceRegistry &dev_reg, std::vector<FsDirectoryEntry> &entries) {
        entries.clear();
        
        std::scoped_lock lock(dev_reg.mutex);
        for (const auto &dev : dev_reg.devices) {
            FsDirectoryEntry entry;
            std::memset(&entry, 0, sizeof(FsDirectoryEntry));
            std::snprintf(entry.name, FS_MAX_PATH, "%s", dev.c_str());
            entry.type = FsDirEntryType_Dir;
            entries.push_back(entry);
        }
    }
    
    void GoToPartitionRoot(FileSystemService &fs_svc, DeviceRegistry &dev_reg, ConfigService &config_svc, std::vector<FsDirectoryEntry> &entries) {
        fs_svc.device = "";
        fs_svc.cwd = "/";
        GetPartitionList(dev_reg, entries);
        SaveCurrentPath(fs_svc, config_svc);
    }
    
    bool SelectPartition(FileSystemService &fs_svc, DeviceRegistry &dev_reg, ConfigService &config_svc, const std::string &partition_name, std::vector<FsDirectoryEntry> &entries) {
        std::scoped_lock lock(dev_reg.mutex);
        
        for (std::size_t i = 0; i < dev_reg.devices.size(); i++) {
            if (dev_reg.devices[i] == partition_name) {
                fs_svc.device = partition_name;
                fs_svc.current_fs = &fs_svc.devices[i];
                fs_svc.cwd = "/";
                
                entries.clear();
                bool ret = GetDirList(fs_svc.device, fs_svc.cwd, entries);
                if (ret) {
                    SaveCurrentPath(fs_svc, config_svc);
                }
                return ret;
            }
        }
        
        return false;
    }
    
    std::string GetDisplayPath(FileSystemService &fs_svc) {
        if (IsAtPartitionRoot(fs_svc)) {
            return "";
        }
        
        std::string path = fs_svc.device;
        path.append(fs_svc.cwd);
        return path;
    }
    
    void SaveCurrentPath(FileSystemService &fs_svc, ConfigService &config_svc) {
        Config::SetLastDevice(config_svc, fs_svc.device, GUI::IsAppletMode());
        Config::SetLastCwd(config_svc, fs_svc.cwd, GUI::IsAppletMode());
        Config::Save(config_svc, fs_svc);
    }
    
    bool RestoreSavedPath(FileSystemService &fs_svc, DeviceRegistry &dev_reg, ConfigService &config_svc, std::vector<FsDirectoryEntry> &entries) {
        if (config_svc.effective.last_device.empty()) {
            GoToPartitionRoot(fs_svc, dev_reg, config_svc, entries);
            return true;
        }
        
        std::scoped_lock lock(dev_reg.mutex);
        bool device_found = false;
        for (std::size_t i = 0; i < dev_reg.devices.size(); i++) {
            if (dev_reg.devices[i] == config_svc.effective.last_device) {
                fs_svc.device = config_svc.effective.last_device;
                fs_svc.current_fs = &fs_svc.devices[i];
                device_found = true;
                break;
            }
        }
        
        if (!device_found) {
            Log::Debug("FS::RestoreSavedPath - device %s not found, going to partition root\n", config_svc.effective.last_device.c_str());
            GoToPartitionRoot(fs_svc, dev_reg, config_svc, entries);
            return false;
        }
        
        fs_svc.cwd = config_svc.effective.last_cwd;
        if (!GetDirList(fs_svc.device, fs_svc.cwd, entries)) {
            Log::Debug("FS::RestoreSavedPath - path %s%s not found, searching for valid parent\n", fs_svc.device.c_str(), fs_svc.cwd.c_str());
            
            while (fs_svc.cwd != "/") {
                std::filesystem::path path = fs_svc.cwd;
                fs_svc.cwd = path.parent_path().string();
                if (fs_svc.cwd.empty()) fs_svc.cwd = "/";
                
                if (GetDirList(fs_svc.device, fs_svc.cwd, entries)) {
                    Log::Debug("FS::RestoreSavedPath - found valid path at %s%s\n", fs_svc.device.c_str(), fs_svc.cwd.c_str());
                    SaveCurrentPath(fs_svc, config_svc);
                    return true;
                }
            }
            
            Log::Debug("FS::RestoreSavedPath - device %s inaccessible, going to partition root\n", fs_svc.device.c_str());
            GoToPartitionRoot(fs_svc, dev_reg, config_svc, entries);
            return false;
        }
        
        return true;
    }

    // ========================================================================
    // Service-independent functions (don't need FS state)
    // ========================================================================

    bool FileExists(const std::string &path) {
        struct stat file_stat = { 0 };
        return (stat(path.c_str(), std::addressof(file_stat)) == 0 && S_ISREG(file_stat.st_mode));
    }
    
    bool DirExists(const std::string &path) {
        struct stat dir_stat = { 0 };
        return (stat(path.c_str(), &dir_stat) == 0);
    }

    bool GetFileSize(const std::string &path, std::size_t &size) {
        struct stat file_stat = { 0 };

        if (stat(path.c_str(), std::addressof(file_stat)) != 0) {
            Log::Error("FS::GetFileSize(%s) failed to stat file.\n", path.c_str());
            return false;
        }

        size = file_stat.st_size;
        return true;
    }
    
    bool GetDirList(const std::string &device, const std::string &path, std::vector<FsDirectoryEntry> &entries) {
        DIR *dir = nullptr;
        struct dirent *d_entry = nullptr;
        
        std::string full_path = device + path;
        dir = opendir(full_path.c_str());
        entries.clear();

        if (dir) {
            FsDirectoryEntry parent_entry;
            std::memset(std::addressof(parent_entry), 0, sizeof(FsDirectoryEntry));
            std::snprintf(parent_entry.name, 3, "..");
            parent_entry.type = FsDirEntryType_Dir;
            entries.push_back(parent_entry);

            while((d_entry = readdir(dir))) {
                FsDirectoryEntry entry;
                std::memset(std::addressof(entry), 0, sizeof(FsDirectoryEntry));

                std::snprintf(entry.name, FS_MAX_PATH, d_entry->d_name);
                entry.type = (d_entry->d_type & DT_DIR) ? FsDirEntryType_Dir : FsDirEntryType_File;
                entry.file_size = 0;

                entries.push_back(entry);
            }

            closedir(dir);
        }
        else {
            Log::Error("FS::GetDirList(%s) to open path.\n", full_path.c_str());
            return false;
        }

        return true;
    }

    bool GetTimeStamp(FsDirectoryEntry &entry, FsTimeStampRaw &timestamp) {
        App& app = GetApp();
        struct stat file_stat = { 0 };
        std::string full_path = BuildPath(app.fs, entry);

        if (stat(full_path.c_str(), std::addressof(file_stat)) != 0) {
            Log::Error("FS::GetTimeStamp(%s) failed to stat file.\n", full_path.c_str());
            return false;
        }

        timestamp.is_valid = 1;
        timestamp.created = file_stat.st_ctime;
        timestamp.modified = file_stat.st_mtime;
        timestamp.accessed = file_stat.st_atime;
        return true;
    }

    bool DeleteRecursive(const std::string &path) {
        DIR *dir = nullptr;
        struct dirent *entry = nullptr;
        dir = opendir(path.c_str());

        if (dir) {
            while((entry = readdir(dir))) {
                std::string filename = entry->d_name;
                if ((filename.compare(".") == 0) || (filename.compare("..") == 0))
                    continue;

                std::string file_path = path;
                file_path.append(path.compare("/") == 0 ? "" : "/");
                file_path.append(filename);

                if (entry->d_type & DT_DIR) {
                    if (!DeleteRecursive(file_path)) {
                        closedir(dir);
                        return false;
                    }
                }
                else {
                    if (remove(file_path.c_str()) != 0) {
                        Log::Error("FS::DeleteRecursive(%s) failed to delete file.\n", file_path.c_str());
                        closedir(dir);
                        return false;
                    }
                }
            }

            closedir(dir);
        }
        else {
            Log::Error("FS::DeleteRecursive(%s) failed to open path.\n", path.c_str());
            return false;
        }

        return (rmdir(path.c_str()) == 0);
    }
    
    bool DeletePath(const std::string &path) {
        struct stat path_stat = { 0 };
        if (stat(path.c_str(), &path_stat) != 0) {
            if (remove(path.c_str()) != 0) {
                Log::Error("FS::DeletePath(%s) failed - path not found.\n", path.c_str());
                return false;
            }
            return true;
        }
        
        if (S_ISDIR(path_stat.st_mode)) {
            return DeleteRecursive(path);
        } else {
            if (remove(path.c_str()) != 0) {
                Log::Error("FS::DeletePath(%s) failed to delete file.\n", path.c_str());
                return false;
            }
            return true;
        }
    }
    
    FileType GetFileType(const std::string &filename) {
        std::string ext = GetFileExt(filename);
        
        auto it = extension_map.find(ext);
        if (it != extension_map.end()) {
            return it->second;
        }
        
        std::string basename = std::filesystem::path(filename).filename().string();
        std::transform(basename.begin(), basename.end(), basename.begin(), ::toupper);
        if (text_basenames.count(basename)) {
            return FileTypeText;
        }
        
        return FileTypeNone;
    }

    std::string GetFileExt(const std::string &filename) {
        std::string ext = std::filesystem::path(filename).extension();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);
        return ext;
    }

    // ========================================================================
    // Legacy API - forwards to App instance
    // ========================================================================
    
    bool DestinationExists(void) {
        return DestinationExists(GetApp().fs);
    }

    size_t CountConflicts(void) {
        App& app = GetApp();
        return CountConflicts(app.fs, app.selection);
    }
    
    void SetConflictHandling(ConflictHandling mode) {
        SetConflictHandling(GetApp().fs, mode);
    }
    
    ConflictHandling GetConflictHandling(void) {
        return GetConflictHandling(GetApp().fs);
    }
    
    void ClearConflictHandling(void) {
        ClearConflictHandling(GetApp().fs);
    }
    
    bool ShouldSkipDueToConflict(const std::string &dest_path) {
        return ShouldSkipDueToConflict(GetApp().fs, dest_path);
    }
    
    bool ChangeDirNext(const std::string &path, std::vector<FsDirectoryEntry> &entries) {
        return ChangeDirNext(GetApp().fs, path, entries);
    }
    
    bool ChangeDirPrev(std::vector<FsDirectoryEntry> &entries) {
        return ChangeDirPrev(GetApp().fs, entries);
    }

    bool Rename(FsDirectoryEntry &entry, const std::string &dest_path) {
        return Rename(GetApp().fs, entry, dest_path);
    }

    bool Delete(FsDirectoryEntry &entry) {
        return Delete(GetApp().fs, entry);
    }

    void Copy(FsDirectoryEntry &entry, const std::string &path) {
        Copy(GetApp().fs, entry, path);
    }

    bool Paste(void) {
        App& app = GetApp();
        return Paste(app.fs, app.config);
    }

    bool Move(void) {
        App& app = GetApp();
        return Move(app.fs, app.config);
    }

    bool WouldCauseRecursiveCopy(void) {
        return WouldCauseRecursiveCopy(GetApp().fs);
    }

    std::string GetCopyEntryFilename(void) {
        return GetCopyEntryFilename(GetApp().fs);
    }
    
    Result SetArchiveBit(const std::string &path) {
        return SetArchiveBit(GetApp().fs, path);
    }
    
    bool HasArchiveBit(const std::string &path) {
        return HasArchiveBit(GetApp().fs, path);
    }
    
    Result GetFreeStorageSpace(s64 &size) {
        return GetFreeStorageSpace(GetApp().fs, size);
    }
    
    Result GetTotalStorageSpace(s64 &size) {
        return GetTotalStorageSpace(GetApp().fs, size);
    }
    
    Result GetUsedStorageSpace(s64 &size) {
        return GetUsedStorageSpace(GetApp().fs, size);
    }

    std::string BuildPath(FsDirectoryEntry &entry) {
        return BuildPath(GetApp().fs, entry);
    }

    std::string BuildPath(const std::string &path, bool device_name) {
        return BuildPath(GetApp().fs, path, device_name);
    }

    void PopulateMetadataCache(const std::vector<FsDirectoryEntry> &entries, std::vector<FileMetadataCache> &cache) {
        PopulateMetadataCache(GetApp().fs, entries, cache);
    }
    
    bool RefreshDirectory(std::vector<FsDirectoryEntry> &entries, std::vector<FileMetadataCache> &cache, bool clear_selection) {
        App& app = GetApp();
        return RefreshDirectory(app.fs, app.selection, entries, cache, clear_selection);
    }
    
    bool IsAtPartitionRoot(void) {
        return IsAtPartitionRoot(GetApp().fs);
    }
    
    void GetPartitionList(std::vector<FsDirectoryEntry> &entries) {
        GetPartitionList(GetApp().device_registry, entries);
    }
    
    void GoToPartitionRoot(std::vector<FsDirectoryEntry> &entries) {
        App& app = GetApp();
        GoToPartitionRoot(app.fs, app.device_registry, app.config, entries);
    }
    
    bool SelectPartition(const std::string &partition_name, std::vector<FsDirectoryEntry> &entries) {
        App& app = GetApp();
        return SelectPartition(app.fs, app.device_registry, app.config, partition_name, entries);
    }
    
    std::string GetDisplayPath(void) {
        return GetDisplayPath(GetApp().fs);
    }
    
    void SaveCurrentPath(void) {
        App& app = GetApp();
        SaveCurrentPath(app.fs, app.config);
    }
    
    bool RestoreSavedPath(std::vector<FsDirectoryEntry> &entries) {
        App& app = GetApp();
        return RestoreSavedPath(app.fs, app.device_registry, app.config, entries);
    }
}
