#include "selection.hpp"
#include "fs.hpp"
#include "log.hpp"
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

// Global selection store instance
SelectionStore g_selection;

std::string SelectionStore::BuildFullPath(const std::string &device, const std::string &cwd, const std::string &name) {
    std::string path = device + cwd;
    if (!path.empty() && path.back() != '/')
        path += "/";
    path += name;
    return path;
}

std::string SelectionStore::GetFilename(const std::string &full_path) {
    size_t pos = full_path.rfind('/');
    if (pos == std::string::npos)
        return full_path;
    return full_path.substr(pos + 1);
}

std::string SelectionStore::GetParentPath(const std::string &full_path) {
    size_t pos = full_path.rfind('/');
    if (pos == std::string::npos)
        return "";
    return full_path.substr(0, pos);
}

bool SelectionStore::IsSelected(const std::string &path) const {
    return selected_paths_.find(path) != selected_paths_.end();
}

bool SelectionStore::HasSelectionsUnder(const std::string &prefix) const {
    if (selected_paths_.empty())
        return false;
    
    // Ensure prefix ends with / for proper prefix matching
    std::string search_prefix = prefix;
    if (!search_prefix.empty() && search_prefix.back() != '/')
        search_prefix += "/";
    
    // Use lower_bound to find first element >= prefix
    auto it = selected_paths_.lower_bound(search_prefix);
    
    // Check if the found element starts with our prefix
    if (it != selected_paths_.end()) {
        return it->compare(0, search_prefix.length(), search_prefix) == 0;
    }
    
    return false;
}

bool SelectionStore::IsInsideSelectedFolder(const std::string &path) const {
    if (selected_paths_.empty())
        return false;
    
    // Check each ancestor path to see if it's selected
    std::string current = path;
    while (!current.empty()) {
        size_t pos = current.rfind('/');
        if (pos == std::string::npos)
            break;
        
        current = current.substr(0, pos);
        if (current.empty())
            break;
            
        // Check if this ancestor is selected
        if (selected_paths_.find(current) != selected_paths_.end())
            return true;
    }
    
    return false;
}

void SelectionStore::Select(const std::string &path) {
    selected_paths_.insert(path);
    UpdateCachedInfo();
}

void SelectionStore::Deselect(const std::string &path) {
    selected_paths_.erase(path);
    // Also remove all ancestor paths so parent folders show partcheck instead of check
    DeselectAncestors(path);
    UpdateCachedInfo();
}

void SelectionStore::DeselectAncestors(const std::string &path) {
    // Remove all ancestor paths from selected_paths_
    // This ensures parent folders show partcheck when a child is deselected
    std::string current = path;
    while (!current.empty()) {
        size_t pos = current.rfind('/');
        if (pos == std::string::npos)
            break;
        
        current = current.substr(0, pos);
        if (current.empty())
            break;
        
        // Don't remove the device root (e.g., "sdmc:")
        if (current.find('/') == std::string::npos)
            break;
        
        // Remove this ancestor if it's selected
        selected_paths_.erase(current);
    }
}

void SelectionStore::Toggle(const std::string &path) {
    auto it = selected_paths_.find(path);
    if (it != selected_paths_.end()) {
        // Deselecting - remove this item and all ancestors
        selected_paths_.erase(it);
        DeselectAncestors(path);
    } else {
        // Selecting - just add this item
        selected_paths_.insert(path);
    }
    UpdateCachedInfo();
}

void SelectionStore::ToggleFolder(const std::string &folder_path) {
    Log::Debug("ToggleFolder: '%s' (IsSelected=%d, HasSelectionsUnder=%d)\n", 
               folder_path.c_str(), IsSelected(folder_path) ? 1 : 0, HasSelectionsUnder(folder_path) ? 1 : 0);
    
    // Check if folder is currently "fully selected" by checking if all contents are selected
    // For simplicity, if ANY item under this folder is selected, and the folder itself is selected,
    // treat it as "selected" and deselect everything. Otherwise, select everything.
    
    if (IsSelected(folder_path)) {
        // Folder is explicitly selected - deselect it and all contents
        Log::Debug("ToggleFolder: Deselecting folder and contents\n");
        DeselectUnder(folder_path);
    } else if (HasSelectionsUnder(folder_path)) {
        // Some items under folder are selected but folder itself isn't
        // Complete the selection by selecting everything
        Log::Debug("ToggleFolder: Completing partial selection\n");
        SelectRecursive(folder_path);
    } else {
        // Nothing selected - select everything
        Log::Debug("ToggleFolder: Selecting folder and all contents\n");
        SelectRecursive(folder_path);
    }
    
    Log::Debug("ToggleFolder: Done. Total selected items: %zu\n", selected_paths_.size());
}

void SelectionStore::SelectRecursive(const std::string &folder_path) {
    // Add the folder itself
    selected_paths_.insert(folder_path);
    Log::Debug("SelectRecursive: Selected folder '%s'\n", folder_path.c_str());
    
    // Parse device and relative path from folder_path (e.g., "sdmc:/photos" -> "sdmc:", "/photos")
    std::string device_part;
    std::string path_part;
    size_t colon_pos = folder_path.find(':');
    if (colon_pos != std::string::npos) {
        device_part = folder_path.substr(0, colon_pos + 1);  // "sdmc:"
        path_part = folder_path.substr(colon_pos + 1);        // "/photos"
    } else {
        // No device prefix - use folder_path as-is
        path_part = folder_path;
    }
    
    // Use FS::GetDirList which is known to work on Switch
    std::vector<FsDirectoryEntry> entries;
    if (!FS::GetDirList(device_part, path_part, entries)) {
        Log::Debug("SelectRecursive: Failed to get directory list for '%s'\n", folder_path.c_str());
        UpdateCachedInfo();
        return;
    }
    
    Log::Debug("SelectRecursive: Found %zu entries in '%s'\n", entries.size(), folder_path.c_str());
    
    // Process each entry
    for (const auto &entry : entries) {
        // Skip ".." entry
        if (std::strncmp(entry.name, "..", 2) == 0)
            continue;
        
        std::string child_path = folder_path;
        if (!child_path.empty() && child_path.back() != '/')
            child_path += "/";
        child_path += entry.name;
        
        selected_paths_.insert(child_path);
        Log::Debug("SelectRecursive: Selected item '%s' (type=%d)\n", child_path.c_str(), entry.type);
        
        // Recurse into subdirectories
        if (entry.type == FsDirEntryType_Dir) {
            SelectRecursive(child_path);
        }
    }
    
    UpdateCachedInfo();
}

void SelectionStore::DeselectUnder(const std::string &prefix) {
    // Build prefix for matching (ensure it ends with /)
    std::string match_prefix = prefix;
    if (!match_prefix.empty() && match_prefix.back() != '/')
        match_prefix += "/";
    
    // Remove the prefix path itself
    selected_paths_.erase(prefix);
    
    // Remove all paths that start with the prefix
    auto it = selected_paths_.lower_bound(match_prefix);
    while (it != selected_paths_.end() && it->compare(0, match_prefix.length(), match_prefix) == 0) {
        it = selected_paths_.erase(it);
    }
    
    // Also remove ancestor paths so parent folders show partcheck instead of check
    DeselectAncestors(prefix);
    
    UpdateCachedInfo();
}

int SelectionStore::GetFolderSelectionState(const std::string &folder_path, const std::vector<FsDirectoryEntry> &entries) const {
    // Count selected vs total direct children (excluding "..")
    int total_children = 0;
    int selected_children = 0;
    
    std::string base_path = folder_path;
    if (!base_path.empty() && base_path.back() != '/')
        base_path += "/";
    
    for (const auto &entry : entries) {
        // Skip ".." entry
        if (std::strncmp(entry.name, "..", 2) == 0)
            continue;
        
        total_children++;
        
        std::string child_path = base_path + entry.name;
        
        if (entry.type == FsDirEntryType_Dir) {
            // For folders, check if it's selected OR has any selections under it
            if (IsSelected(child_path) || HasSelectionsUnder(child_path))
                selected_children++;
        } else {
            // For files, just check if selected
            if (IsSelected(child_path))
                selected_children++;
        }
    }
    
    if (total_children == 0)
        return 0;  // Empty folder - treat as none selected
    
    if (selected_children == 0)
        return 0;  // None selected
    else if (selected_children == total_children)
        return 2;  // All selected
    else
        return 1;  // Partial
}

void SelectionStore::Clear() {
    selected_paths_.clear();
    device_.clear();
    selection_directory_.clear();
}

void SelectionStore::SelectAll(const std::string &base_path, const std::vector<FsDirectoryEntry> &entries) {
    for (const auto &entry : entries) {
        // Skip ".." entry
        if (std::strncmp(entry.name, "..", 2) == 0)
            continue;
        
        std::string full_path = base_path;
        if (!full_path.empty() && full_path.back() != '/')
            full_path += "/";
        full_path += entry.name;
        
        selected_paths_.insert(full_path);
    }
    UpdateCachedInfo();
}

size_t SelectionStore::Count() const {
    return selected_paths_.size();
}

bool SelectionStore::HasSelections() const {
    return !selected_paths_.empty();
}

const std::string& SelectionStore::GetDevice() const {
    return device_;
}

const std::string& SelectionStore::GetSelectionDirectory() const {
    return selection_directory_;
}

const std::set<std::string>& SelectionStore::GetSelectedPaths() const {
    return selected_paths_;
}

void SelectionStore::UpdateCachedInfo() {
    if (selected_paths_.empty()) {
        device_.clear();
        selection_directory_.clear();
        return;
    }
    
    // Extract device from first selected path
    const std::string &first_path = *selected_paths_.begin();
    size_t colon_pos = first_path.find(':');
    if (colon_pos != std::string::npos) {
        device_ = first_path.substr(0, colon_pos + 1);
    }
    
    // Find common directory prefix of all selections
    selection_directory_ = GetParentPath(first_path);
    
    for (const auto &path : selected_paths_) {
        std::string parent = GetParentPath(path);
        
        // Find common prefix between selection_directory_ and parent
        size_t i = 0;
        while (i < selection_directory_.length() && i < parent.length() && 
               selection_directory_[i] == parent[i]) {
            i++;
        }
        
        // Trim to last complete path component
        if (i < selection_directory_.length()) {
            size_t last_slash = selection_directory_.rfind('/', i);
            if (last_slash != std::string::npos)
                selection_directory_ = selection_directory_.substr(0, last_slash);
            else
                selection_directory_.clear();
        }
    }
}
