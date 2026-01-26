#pragma once

#include <string>
#include <switch.h>
#include <vector>
#include "services.hpp"

// Legacy extern declarations - forward to App instance
extern FsFileSystem *&fs;
extern FsFileSystem (&devices)[FileSystemMax];

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
    bool ChangeDirNext(FileSystemService &fs_svc, const std::string &path, std::vector<FsDirectoryEntry> &entries);
    bool ChangeDirPrev(FileSystemService &fs_svc, std::vector<FsDirectoryEntry> &entries);
    bool GetTimeStamp(FsDirectoryEntry &entry, FsTimeStampRaw &timestamp);
    bool Rename(FileSystemService &fs_svc, FsDirectoryEntry &entry, const std::string &dest_path);
    bool Delete(FileSystemService &fs_svc, FsDirectoryEntry &entry);
    bool DeleteRecursive(const std::string &path);
    bool DeletePath(const std::string &path);
    void Copy(FileSystemService &fs_svc, FsDirectoryEntry &entry, const std::string &path);
    bool Paste(FileSystemService &fs_svc, ConfigService &config_svc);
    bool Move(FileSystemService &fs_svc, ConfigService &config_svc);
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
    
    // ========================================================================
    // Legacy API - uses global App instance (for transition)
    // ========================================================================
    bool DestinationExists(void);
    size_t CountConflicts(void);
    void SetConflictHandling(ConflictHandling mode);
    ConflictHandling GetConflictHandling(void);
    void ClearConflictHandling(void);
    bool ShouldSkipDueToConflict(const std::string &dest_path);
    bool ChangeDirNext(const std::string &path, std::vector<FsDirectoryEntry> &entries);
    bool ChangeDirPrev(std::vector<FsDirectoryEntry> &entries);
    bool Rename(FsDirectoryEntry &entry, const std::string &dest_path);
    bool Delete(FsDirectoryEntry &entry);
    void Copy(FsDirectoryEntry &entry, const std::string &path);
    bool Paste(void);
    bool Move(void);
    bool WouldCauseRecursiveCopy(void);
    std::string GetCopyEntryFilename(void);
    Result SetArchiveBit(const std::string &path);
    bool HasArchiveBit(const std::string &path);
    Result GetFreeStorageSpace(s64 &size);
    Result GetTotalStorageSpace(s64 &size);
    Result GetUsedStorageSpace(s64 &size);
    std::string BuildPath(FsDirectoryEntry &entry);
    std::string BuildPath(const std::string &path, bool device_name);
    void PopulateMetadataCache(const std::vector<FsDirectoryEntry> &entries, std::vector<FileMetadataCache> &cache);
    bool RefreshDirectory(std::vector<FsDirectoryEntry> &entries, std::vector<FileMetadataCache> &cache, bool clear_selection);
    bool IsAtPartitionRoot(void);
    void GoToPartitionRoot(std::vector<FsDirectoryEntry> &entries);
    void GetPartitionList(std::vector<FsDirectoryEntry> &entries);
    bool SelectPartition(const std::string &partition_name, std::vector<FsDirectoryEntry> &entries);
    std::string GetDisplayPath(void);
    void SaveCurrentPath(void);
    bool RestoreSavedPath(std::vector<FsDirectoryEntry> &entries);
}
