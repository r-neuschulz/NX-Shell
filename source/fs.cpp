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
#include "language.hpp"
#include "log.hpp"
#include "popups.hpp"
#include "selection.hpp"

// Global vars
FsFileSystem *fs;
FsFileSystem devices[FileSystemMax];
std::string cwd = "/";
std::string device = "sdmc:";

// External globals from filebrowser.cpp (for partition list)
extern std::vector<std::string> devices_list;
extern std::recursive_mutex devices_list_mutex;

namespace FS {

    typedef struct {
        std::string path;
        std::string filename;
        bool is_directory = false;
    } FSCopyEntry;
    
    FSCopyEntry fs_copy_entry;
    
    // Conflict handling state for multi-file operations
    static ConflictHandling conflict_handling_mode = ConflictHandling_Ask;

    size_t CountConflicts(void) {
        const auto &selected_paths = g_selection.GetSelectedPaths();
        size_t conflicts = 0;
        
        for (const auto &full_path : selected_paths) {
            std::string filename = SelectionStore::GetFilename(full_path);
            if (filename == "..")
                continue;
            
            std::string dest_path = FS::BuildPath(filename, true);
            struct stat dest_stat = { 0 };
            if (stat(dest_path.c_str(), &dest_stat) == 0) {
                conflicts++;
            }
        }
        
        return conflicts;
    }
    
    void SetConflictHandling(ConflictHandling mode) {
        conflict_handling_mode = mode;
    }
    
    ConflictHandling GetConflictHandling(void) {
        return conflict_handling_mode;
    }
    
    void ClearConflictHandling(void) {
        conflict_handling_mode = ConflictHandling_Ask;
    }
    
    bool ShouldSkipDueToConflict(const std::string &dest_path) {
        if (conflict_handling_mode != ConflictHandling_SkipAll)
            return false;
        
        struct stat dest_stat = { 0 };
        return (stat(dest_path.c_str(), &dest_stat) == 0);
    }

    bool FileExists(const std::string &path) {
        struct stat file_stat = { 0 };
        return (stat(path.c_str(), std::addressof(file_stat)) == 0 && S_ISREG(file_stat.st_mode));
    }
    
    bool DirExists(const std::string &path) {
        struct stat dir_stat = { 0 };
        return (stat(path.c_str(), &dir_stat) == 0);
    }

    bool DestinationExists(void) {
        if (fs_copy_entry.filename.empty())
            return false;
        
        std::string dest_path = FS::BuildPath(fs_copy_entry.filename, true);
        struct stat dest_stat = { 0 };
        return (stat(dest_path.c_str(), std::addressof(dest_stat)) == 0);
    }

    bool GetFileSize(const std::string &path, std::size_t &size) {
        struct stat file_stat = { 0 };
        std::string full_path = FS::BuildPath(path, true);

        if (stat(full_path.c_str(), std::addressof(file_stat)) != 0) {
            Log::Error("FS::GetFileSize(%s) failed to stat file.\n", full_path.c_str());
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
                entry.type = (d_entry->d_type & DT_DIR)? FsDirEntryType_Dir : FsDirEntryType_File;
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

    static bool ChangeDir(const std::string &path, std::vector<FsDirectoryEntry> &entries) {
        std::vector<FsDirectoryEntry> new_entries;
        const std::string new_path = path;
        cwd = path;
        
        bool ret = FS::GetDirList(device, new_path, new_entries);
        
        if (ret) {
            // Save the new path to config
            SaveCurrentPath();
        }
        
        entries.clear();
        entries = new_entries;
        return ret;
    }
    
    bool ChangeDirNext(const std::string &path, std::vector<FsDirectoryEntry> &entries) {
        return FS::ChangeDir(FS::BuildPath(path, false), entries);
    }
    
    bool ChangeDirPrev(std::vector<FsDirectoryEntry> &entries) {
        // We are already at the root.
        if (cwd.compare("/") == 0)
            return false;
        
        std::filesystem::path path = cwd;
        std::string parent_path = path.parent_path();
        return FS::ChangeDir(parent_path.empty()? cwd : parent_path, entries);
    }

    bool GetTimeStamp(FsDirectoryEntry &entry, FsTimeStampRaw &timestamp) {
        struct stat file_stat = { 0 };
        std::string full_path = FS::BuildPath(entry);

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

    void PopulateMetadataCache(const std::vector<FsDirectoryEntry> &entries, std::vector<FileMetadataCache> &cache) {
        cache.clear();
        
        // Pre-allocate with default-initialized structs
        FileMetadataCache empty_cache;
        empty_cache.file_size = 0;
        empty_cache.modified_time = 0;
        empty_cache.has_archive_bit = false;
        empty_cache.valid = false;
        cache.assign(entries.size(), empty_cache);
        
        for (size_t i = 0; i < entries.size(); i++) {
            // Skip ".." entry
            if (std::strncmp(entries[i].name, "..", 2) == 0) {
                continue;  // Already initialized as invalid
            }
            
            std::string full_path = FS::BuildPath(const_cast<FsDirectoryEntry&>(entries[i]));
            struct stat file_stat = { 0 };
            
            if (stat(full_path.c_str(), std::addressof(file_stat)) == 0) {
                cache[i].file_size = file_stat.st_size;
                cache[i].modified_time = file_stat.st_mtime;
                cache[i].valid = true;
                
                // Check archive bit for directories
                if (entries[i].type == FsDirEntryType_Dir) {
                    cache[i].has_archive_bit = FS::HasArchiveBit(full_path);
                }
            }
            // If stat fails, entry stays as invalid (already set)
        }
    }
    
    bool RefreshDirectory(std::vector<FsDirectoryEntry> &entries, std::vector<FileMetadataCache> &cache, bool clear_selection) {
        if (!FS::GetDirList(device, cwd, entries)) {
            Log::Error("FS::RefreshDirectory() failed to get directory list.\n");
            return false;
        }
        FS::PopulateMetadataCache(entries, cache);
        if (clear_selection)
            g_selection.Clear();
        return true;
    }

    bool Rename(FsDirectoryEntry &entry, const std::string &dest_path) {
        std::string src_path = FS::BuildPath(entry);
        std::string full_dest_path = FS::BuildPath(dest_path, true);

        if (rename(src_path.c_str(), full_dest_path.c_str()) != 0) {
            Log::Error("FS::Rename(%s, %s) failed.\n", src_path.c_str(), dest_path.c_str());
            return false;
        }
        
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
                file_path.append(path.compare("/") == 0? "" : "/");
                file_path.append(filename);

                if (entry->d_type & DT_DIR) {
                    if (!FS::DeleteRecursive(file_path)) {
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
    
    bool Delete(FsDirectoryEntry &entry) {
        std::string full_path = FS::BuildPath(entry);

        if (entry.type == FsDirEntryType_Dir) {
            if (!FS::DeleteRecursive(full_path)) {
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
    
    bool DeletePath(const std::string &path) {
        struct stat path_stat = { 0 };
        if (stat(path.c_str(), &path_stat) != 0) {
            // Path doesn't exist or stat failed - try remove as file anyway
            if (remove(path.c_str()) != 0) {
                Log::Error("FS::DeletePath(%s) failed - path not found.\n", path.c_str());
                return false;
            }
            return true;
        }
        
        if (S_ISDIR(path_stat.st_mode)) {
            return FS::DeleteRecursive(path);
        } else {
            if (remove(path.c_str()) != 0) {
                Log::Error("FS::DeletePath(%s) failed to delete file.\n", path.c_str());
                return false;
            }
            return true;
        }
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
            // This may fail or not, but we don't care -> make the dir if it doesn't exist, otherwise continue.
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
                    copy_result = FS::CopyDir(src.c_str(), dest.c_str()); // Copy Folder (via recursion)
                else
                    copy_result = FS::CopyFile(src.c_str(), dest.c_str()); // Copy File
                
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

    void Copy(FsDirectoryEntry &entry, const std::string &path) {
        std::string full_path = path;
        full_path.append(path.compare("/") == 0? "" : "/");
        full_path.append(entry.name);
        
        if ((std::strncmp(entry.name, "..", 2)) != 0) {
            fs_copy_entry.path = full_path;
            fs_copy_entry.filename = entry.name;
            
            if (entry.type == FsDirEntryType_Dir)
                fs_copy_entry.is_directory = true;
        }
    }

    bool WouldCauseRecursiveCopy(void) {
        // Only directories can cause recursive copy issues
        if (!fs_copy_entry.is_directory)
            return false;
        
        std::string src_path = fs_copy_entry.path;
        std::string dest_parent = device + cwd;
        
        // Normalize paths (ensure no trailing slashes for comparison)
        if (!src_path.empty() && src_path.back() == '/')
            src_path.pop_back();
        if (!dest_parent.empty() && dest_parent.back() == '/')
            dest_parent.pop_back();
        
        // Check if destination parent is inside the source
        // e.g., src="sdmc:/photos", dest_parent="sdmc:/photos/vacation" -> recursive
        // We need to check if dest_parent starts with src_path + "/"
        std::string src_with_slash = src_path + "/";
        if (dest_parent.compare(0, src_with_slash.length(), src_with_slash) == 0) {
            return true;
        }
        
        return false;
    }

    bool Paste(void) {
        bool ret = false;
        std::string path = FS::BuildPath(fs_copy_entry.filename, true);
        
        if (fs_copy_entry.is_directory)
            ret = FS::CopyDir(fs_copy_entry.path, path);
        else
            ret = FS::CopyFile(fs_copy_entry.path, path);

        fs_copy_entry = {};
        return ret;
    }

    bool Move(void) {
        std::string path = FS::BuildPath(fs_copy_entry.filename, true);

        if (rename(fs_copy_entry.path.c_str(), path.c_str()) != 0) {
            Log::Error("FS::Move(%s, %s) failed.\n", fs_copy_entry.path.c_str(), path.c_str());
            return false;
        }

        fs_copy_entry = {};
        return true;
    }

    std::string GetCopyEntryFilename(void) {
        return fs_copy_entry.filename;
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
    
    FileType GetFileType(const std::string &filename) {
        std::string ext = FS::GetFileExt(filename);
        
        // Lookup extension in map
        auto it = extension_map.find(ext);
        if (it != extension_map.end()) {
            return it->second;
        }
        
        // Check for common extensionless text files
        std::string basename = std::filesystem::path(filename).filename().string();
        std::transform(basename.begin(), basename.end(), basename.begin(), ::toupper);
        if (text_basenames.count(basename)) {
            return FileTypeText;
        }
        
        return FileTypeNone;
    }
    
    Result SetArchiveBit(const std::string &path) {
        Result ret = 0;

        char fs_path[FS_MAX_PATH];
        std::snprintf(fs_path, FS_MAX_PATH, path.c_str());

        if (R_FAILED(ret = fsFsSetConcatenationFileAttribute(std::addressof(devices[FileSystemSDMC]), fs_path))) {
            Log::Error("fsFsSetConcatenationFileAttribute(%s) failed: 0x%x\n", path.c_str(), ret);
            return ret;
        }
        
        return 0;
    }
    
    bool HasArchiveBit(const std::string &path) {
        // On Nintendo Switch, the archive bit is the concatenation file attribute
        // A directory with this attribute set is treated as a single large file
        // We check this by querying if the path is a valid concatenation file
        char fs_path[FS_MAX_PATH];
        std::snprintf(fs_path, FS_MAX_PATH, "%s", path.c_str());
        
        // Determine which filesystem to use based on path prefix
        FsFileSystem *target_fs = nullptr;
        const char *rel_path = fs_path;
        
        if (std::strncmp(fs_path, "sdmc:", 5) == 0) {
            target_fs = std::addressof(devices[FileSystemSDMC]);
            rel_path = fs_path + 5;
        }
        else if (std::strncmp(fs_path, "safe:", 5) == 0) {
            target_fs = std::addressof(devices[FileSystemSafe]);
            rel_path = fs_path + 5;
        }
        else if (std::strncmp(fs_path, "user:", 5) == 0) {
            target_fs = std::addressof(devices[FileSystemUser]);
            rel_path = fs_path + 5;
        }
        else if (std::strncmp(fs_path, "system:", 7) == 0) {
            target_fs = std::addressof(devices[FileSystemSystem]);
            rel_path = fs_path + 7;
        }
        else {
            // Unknown device prefix, use current fs as fallback
            target_fs = fs;
        }
        
        // Safety check - filesystem must be valid
        if (target_fs == nullptr) {
            return false;
        }
        
        FsDirEntryType entry_type;
        Result ret = fsFsGetEntryType(target_fs, rel_path, &entry_type);
        if (R_FAILED(ret))
            return false;
        
        // Check if it's a directory first (archive bit is only meaningful for directories)
        struct stat file_stat = { 0 };
        if (stat(path.c_str(), &file_stat) != 0)
            return false;
        
        // If stat says it's a directory but the FS says it's a file, it has the archive bit set
        if (S_ISDIR(file_stat.st_mode) && entry_type == FsDirEntryType_File)
            return true;
        
        return false;
    }
    
    Result GetFreeStorageSpace(s64 &size) {
        Result ret = 0;
        
        if (R_FAILED(ret = fsFsGetFreeSpace(fs, "/", std::addressof(size)))) {
            Log::Error("fsFsGetFreeSpace() failed: 0x%x\n", ret);
            return ret;
        }
        
        return 0;
    }
    
    Result GetTotalStorageSpace(s64 &size) {
        Result ret = 0;
        
        if (R_FAILED(ret = fsFsGetTotalSpace(fs, "/", std::addressof(size)))) {
            Log::Error("fsFsGetTotalSpace() failed: 0x%x\n", ret);
            return ret;
        }
        
        return 0;
    }
    
    Result GetUsedStorageSpace(s64 &size) {
        Result ret = 0;
        s64 free_size = 0, total_size = 0;
        
        if (R_FAILED(ret = FS::GetFreeStorageSpace(free_size)))
            return ret;
            
        if (R_FAILED(ret = FS::GetTotalStorageSpace(total_size)))
            return ret;
        
        size = (total_size - free_size);
        return 0;
    }

    std::string GetFileExt(const std::string &filename) {
        std::string ext = std::filesystem::path(filename).extension();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);
        return ext;
    }

    std::string BuildPath(FsDirectoryEntry &entry) {
        std::string path_next = device;
        path_next.append(cwd);
        path_next.append((cwd.compare("/") == 0)? "" : "/");
        path_next.append(entry.name);
        return path_next;
    }

    std::string BuildPath(const std::string &path, bool device_name) {
        std::string path_next = "";

        if (device_name)
            path_next.append(device);
        
        path_next.append(cwd);
        path_next.append((cwd.compare("/") == 0)? "" : "/");
        path_next.append(path);
        return path_next;
    }
    
    bool IsAtPartitionRoot(void) {
        // Empty device string indicates we're at the partition root
        return device.empty();
    }
    
    void GetPartitionList(std::vector<FsDirectoryEntry> &entries) {
        entries.clear();
        
        std::scoped_lock lock(::devices_list_mutex);
        for (const auto &dev : ::devices_list) {
            FsDirectoryEntry entry;
            std::memset(&entry, 0, sizeof(FsDirectoryEntry));
            std::snprintf(entry.name, FS_MAX_PATH, "%s", dev.c_str());
            entry.type = FsDirEntryType_Dir;  // Treat partitions as directories
            entries.push_back(entry);
        }
    }
    
    void GoToPartitionRoot(std::vector<FsDirectoryEntry> &entries) {
        device = "";  // Empty device indicates partition root
        cwd = "/";
        GetPartitionList(entries);
        SaveCurrentPath();
    }
    
    bool SelectPartition(const std::string &partition_name, std::vector<FsDirectoryEntry> &entries) {
        std::scoped_lock lock(::devices_list_mutex);
        
        // Find the partition in the device list
        for (std::size_t i = 0; i < ::devices_list.size(); i++) {
            if (::devices_list[i] == partition_name) {
                device = partition_name;
                fs = std::addressof(devices[i]);
                cwd = "/";
                
                entries.clear();
                bool ret = FS::GetDirList(device, cwd, entries);
                if (ret) {
                    SaveCurrentPath();
                }
                return ret;
            }
        }
        
        return false;
    }
    
    std::string GetDisplayPath(void) {
        if (IsAtPartitionRoot()) {
            return "";  // Empty string at partition root
        }
        
        // Return device + cwd, e.g., "sdmc:/folder/subfolder"
        std::string path = device;
        path.append(cwd);
        return path;
    }
    
    void SaveCurrentPath(void) {
        Config::SetLastDevice(device);
        Config::SetLastCwd(cwd);
        Config::Save(cfg);
    }
    
    bool RestoreSavedPath(std::vector<FsDirectoryEntry> &entries) {
        // If saved device is empty, user was at partition root
        if (eff.last_device.empty()) {
            GoToPartitionRoot(entries);
            return true;
        }
        
        // Find the device in the device list and set up fs pointer
        std::scoped_lock lock(::devices_list_mutex);
        bool device_found = false;
        for (std::size_t i = 0; i < ::devices_list.size(); i++) {
            if (::devices_list[i] == eff.last_device) {
                device = eff.last_device;
                fs = std::addressof(devices[i]);
                device_found = true;
                break;
            }
        }
        
        if (!device_found) {
            // Device no longer exists (e.g., USB was removed) - go to partition root
            Log::Debug("FS::RestoreSavedPath - device %s not found, going to partition root\n", eff.last_device.c_str());
            GoToPartitionRoot(entries);
            return false;
        }
        
        // Try to open the saved path
        cwd = eff.last_cwd;
        if (!GetDirList(device, cwd, entries)) {
            // Path doesn't exist - try going up until we find a valid directory
            Log::Debug("FS::RestoreSavedPath - path %s%s not found, searching for valid parent\n", device.c_str(), cwd.c_str());
            
            while (cwd != "/") {
                std::filesystem::path path = cwd;
                cwd = path.parent_path().string();
                if (cwd.empty()) cwd = "/";
                
                if (GetDirList(device, cwd, entries)) {
                    Log::Debug("FS::RestoreSavedPath - found valid path at %s%s\n", device.c_str(), cwd.c_str());
                    // Save the corrected path
                    SaveCurrentPath();
                    return true;
                }
            }
            
            // Even root failed - device might be inaccessible, go to partition root
            Log::Debug("FS::RestoreSavedPath - device %s inaccessible, going to partition root\n", device.c_str());
            GoToPartitionRoot(entries);
            return false;
        }
        
        return true;
    }
}
