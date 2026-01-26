#pragma once

#include <set>
#include <string>
#include <vector>
#include <switch.h>
#include "services.hpp"

// SelectionStore class wrapping SelectionService for compatibility
class SelectionStore {
public:
    explicit SelectionStore(SelectionService &svc);
    
    bool IsSelected(const std::string &path) const;
    bool HasSelectionsUnder(const std::string &prefix) const;
    bool IsInsideSelectedFolder(const std::string &path) const;
    
    void Select(const std::string &path);
    void Deselect(const std::string &path);
    void Toggle(const std::string &path);
    void ToggleFolder(const std::string &folder_path);
    void SelectRecursive(const std::string &folder_path);
    void DeselectUnder(const std::string &prefix);
    int GetFolderSelectionState(const std::string &folder_path, const std::vector<FsDirectoryEntry> &entries) const;
    void Clear();
    void SelectAll(const std::string &base_path, const std::vector<FsDirectoryEntry> &entries);
    
    size_t Count() const;
    bool HasSelections() const;
    const std::string& GetDevice() const;
    const std::string& GetSelectionDirectory() const;
    const std::set<std::string>& GetSelectedPaths() const;
    
    static std::string BuildFullPath(const std::string &device, const std::string &cwd, const std::string &name);
    static std::string GetFilename(const std::string &full_path);
    static std::string GetParentPath(const std::string &full_path);

private:
    SelectionService *svc_;
    
    void UpdateCachedInfo();
    void DeselectAncestors(const std::string &path);
};
