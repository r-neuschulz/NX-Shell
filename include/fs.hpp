#pragma once

#include <string>
#include <switch.h>
#include <vector>

typedef enum FileType {
    FileTypeNone,
    FileTypeArchive,
    FileTypeImage,
    FileTypeText,
    FileTypeBinary  // Files that should default to hex mode (.bin, .exe, .nro, etc.)
} FileType;

typedef enum FileSystemDevices {
    FileSystemSDMC,
    FileSystemSafe,
    FileSystemUser,
    FileSystemSystem,
    FileSystemMax
} FileSystemDevices;

extern FsFileSystem *fs;
extern FsFileSystem devices[FileSystemMax];

// Cached file metadata to avoid per-frame stat() calls
struct FileMetadataCache {
    std::size_t file_size = 0;
    s64 modified_time = 0;
    bool has_archive_bit = false;
    bool valid = false;  // Whether metadata was successfully loaded
};

// Conflict handling mode for multi-file copy/move operations
enum ConflictHandling {
    ConflictHandling_Ask,       // Default: ask for single files
    ConflictHandling_ReplaceAll,
    ConflictHandling_SkipAll
};

namespace FS {
    bool FileExists(const std::string &path);
    bool DirExists(const std::string &path);
    bool DestinationExists(void);  // Check if copy/move destination already exists
    
    // Multi-file conflict detection and handling
    size_t CountConflicts(void);  // Count how many selected files would conflict
    void SetConflictHandling(ConflictHandling mode);
    ConflictHandling GetConflictHandling(void);
    void ClearConflictHandling(void);  // Reset to Ask mode
    bool ShouldSkipDueToConflict(const std::string &dest_path);  // Check if file should be skipped
    bool GetFileSize(const std::string &path, std::size_t &size);
    bool GetDirList(const std::string &device, const std::string &path, std::vector<FsDirectoryEntry> &entries);
    bool ChangeDirNext(const std::string &path, std::vector<FsDirectoryEntry> &entries);
    bool ChangeDirPrev(std::vector<FsDirectoryEntry> &entries);
    bool GetTimeStamp(FsDirectoryEntry &entry, FsTimeStampRaw &timestamp);
    bool Rename(FsDirectoryEntry &entry, const std::string &dest_path);
    bool Delete(FsDirectoryEntry &entry);
    bool DeleteRecursive(const std::string &path);  // Delete directory recursively by full path
    bool DeletePath(const std::string &path);       // Delete file or directory by full path (auto-detects type)
    void Copy(FsDirectoryEntry &entry, const std::string &path);
    bool Paste(void);
    bool Move(void);
    bool WouldCauseRecursiveCopy(void);
    std::string GetCopyEntryFilename(void);  // Get the filename of the pending copy/move entry
    FileType GetFileType(const std::string &filename);
    Result SetArchiveBit(const std::string &path);
    bool HasArchiveBit(const std::string &path);
    Result GetFreeStorageSpace(s64 &size);
    Result GetTotalStorageSpace(s64 &size);
    Result GetUsedStorageSpace(s64 &size);
    std::string BuildPath(FsDirectoryEntry &entry);
    std::string BuildPath(const std::string &path, bool device_name);
    std::string GetFileExt(const std::string &filename);
    
    // Populate metadata cache for all entries (call once after GetDirList)
    void PopulateMetadataCache(const std::vector<FsDirectoryEntry> &entries, std::vector<FileMetadataCache> &cache);
    
    // Refresh current directory listing and optionally clear selection
    // Consolidates the common pattern: GetDirList + PopulateMetadataCache + Clear
    void RefreshDirectory(std::vector<FsDirectoryEntry> &entries, std::vector<FileMetadataCache> &cache, bool clear_selection);
    
    // Partition root support (virtual root showing all available partitions)
    bool IsAtPartitionRoot(void);
    void GoToPartitionRoot(std::vector<FsDirectoryEntry> &entries);
    void GetPartitionList(std::vector<FsDirectoryEntry> &entries);
    bool SelectPartition(const std::string &partition_name, std::vector<FsDirectoryEntry> &entries);
    
    // Get full path string for display (device + cwd, or empty at partition root)
    std::string GetDisplayPath(void);
    
    // Save current path to config (call after navigation changes)
    void SaveCurrentPath(void);
    
    // Restore path from config and validate it exists
    // Returns true if path was restored, false if fell back to partition root
    bool RestoreSavedPath(std::vector<FsDirectoryEntry> &entries);
}
