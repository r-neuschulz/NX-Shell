#pragma once

#include <string>
#include <switch.h>
#include <vector>
#include "services.hpp"

namespace FS {
    bool FileExists(const std::string &path);
    bool DirExists(const std::string &path);
    bool DestinationExists(FileSystemService &fs_svc);
    
    // Multi-file conflict detection and handling
    size_t CountConflicts(FileSystemService &fs_svc, SelectionService &sel_svc);
    void SetConflictHandling(FileSystemService &fs_svc, ConflictHandling mode);
    ConflictHandling GetConflictHandling(FileSystemService &fs_svc);
    void ClearConflictHandling(FileSystemService &fs_svc);
    bool ShouldSkipDueToConflict(FileSystemService &fs_svc, const std::string &dest_path);
    
    bool GetFileSize(const std::string &path, std::size_t &size);
    bool GetDirList(const std::string &device, const std::string &path, std::vector<FsDirectoryEntry> &entries);
    bool ChangeDirNext(FileSystemService &fs_svc, ConfigService &config_svc, const std::string &path, std::vector<FsDirectoryEntry> &entries);
    bool ChangeDirPrev(FileSystemService &fs_svc, ConfigService &config_svc, std::vector<FsDirectoryEntry> &entries);
    bool GetTimeStamp(FileSystemService &fs_svc, FsDirectoryEntry &entry, FsTimeStampRaw &timestamp);
    bool Rename(FileSystemService &fs_svc, FsDirectoryEntry &entry, const std::string &dest_path);
    bool Delete(FileSystemService &fs_svc, FsDirectoryEntry &entry);
    bool DeleteRecursive(const std::string &path);
    bool DeletePath(const std::string &path);
    void Copy(FileSystemService &fs_svc, FsDirectoryEntry &entry, const std::string &path);
    bool Paste(App &app);
    bool Move(App &app);
    bool WouldCauseRecursiveCopy(FileSystemService &fs_svc);
    std::string GetCopyEntryFilename(FileSystemService &fs_svc);
    FileType GetFileType(const std::string &filename);
    Result SetArchiveBit(FileSystemService &fs_svc, const std::string &path);
    bool HasArchiveBit(FileSystemService &fs_svc, const std::string &path);
    Result GetFreeStorageSpace(FileSystemService &fs_svc, s64 &size);
    Result GetTotalStorageSpace(FileSystemService &fs_svc, s64 &size);
    Result GetUsedStorageSpace(FileSystemService &fs_svc, s64 &size);
    std::string BuildPath(FileSystemService &fs_svc, FsDirectoryEntry &entry);
    std::string BuildPath(FileSystemService &fs_svc, const std::string &path, bool device_name);
    std::string GetFileExt(const std::string &filename);
    
    // Populate metadata cache for all entries
    void PopulateMetadataCache(FileSystemService &fs_svc, const std::vector<FsDirectoryEntry> &entries, std::vector<FileMetadataCache> &cache);
    
    // Refresh current directory listing
    bool RefreshDirectory(FileSystemService &fs_svc, SelectionService &sel_svc, std::vector<FsDirectoryEntry> &entries, std::vector<FileMetadataCache> &cache, bool clear_selection);
    
    // Partition root support
    bool IsAtPartitionRoot(FileSystemService &fs_svc);
    void GoToPartitionRoot(FileSystemService &fs_svc, DeviceRegistry &dev_reg, ConfigService &config_svc, std::vector<FsDirectoryEntry> &entries);
    void GetPartitionList(DeviceRegistry &dev_reg, std::vector<FsDirectoryEntry> &entries);
    bool SelectPartition(FileSystemService &fs_svc, DeviceRegistry &dev_reg, ConfigService &config_svc, const std::string &partition_name, std::vector<FsDirectoryEntry> &entries);
    
    // Path utilities
    std::string GetDisplayPath(FileSystemService &fs_svc);
    void SaveCurrentPath(FileSystemService &fs_svc, ConfigService &config_svc);
    bool RestoreSavedPath(FileSystemService &fs_svc, DeviceRegistry &dev_reg, ConfigService &config_svc, std::vector<FsDirectoryEntry> &entries);
}
