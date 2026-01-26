#include "selection.hpp"
#include "fs.hpp"
#include "log.hpp"
#include "services.hpp"
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

// Global selection store instance - wraps App::selection
SelectionStore g_selection(GetApp().selection);

SelectionStore::SelectionStore() : svc_(&GetApp().selection) {}

SelectionStore::SelectionStore(SelectionService &svc) : svc_(&svc) {}

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
    return svc_->selected_paths.find(path) != svc_->selected_paths.end();
}

bool SelectionStore::HasSelectionsUnder(const std::string &prefix) const {
    if (svc_->selected_paths.empty())
        return false;
    
    std::string search_prefix = prefix;
    if (!search_prefix.empty() && search_prefix.back() != '/')
        search_prefix += "/";
    
    auto it = svc_->selected_paths.lower_bound(search_prefix);
    
    if (it != svc_->selected_paths.end()) {
        return it->compare(0, search_prefix.length(), search_prefix) == 0;
    }
    
    return false;
}

bool SelectionStore::IsInsideSelectedFolder(const std::string &path) const {
    if (svc_->selected_paths.empty())
        return false;
    
    std::string current = path;
    while (!current.empty()) {
        size_t pos = current.rfind('/');
        if (pos == std::string::npos)
            break;
        
        current = current.substr(0, pos);
        if (current.empty())
            break;
            
        if (svc_->selected_paths.find(current) != svc_->selected_paths.end())
            return true;
    }
    
    return false;
}

void SelectionStore::Select(const std::string &path) {
    svc_->selected_paths.insert(path);
    UpdateCachedInfo();
}

void SelectionStore::Deselect(const std::string &path) {
    svc_->selected_paths.erase(path);
    DeselectAncestors(path);
    UpdateCachedInfo();
}

void SelectionStore::DeselectAncestors(const std::string &path) {
    std::string current = path;
    while (!current.empty()) {
        size_t pos = current.rfind('/');
        if (pos == std::string::npos)
            break;
        
        current = current.substr(0, pos);
        if (current.empty())
            break;
        
        if (current.find('/') == std::string::npos)
            break;
        
        svc_->selected_paths.erase(current);
    }
}

void SelectionStore::Toggle(const std::string &path) {
    auto it = svc_->selected_paths.find(path);
    if (it != svc_->selected_paths.end()) {
        svc_->selected_paths.erase(it);
        DeselectAncestors(path);
    } else {
        svc_->selected_paths.insert(path);
    }
    UpdateCachedInfo();
}

void SelectionStore::ToggleFolder(const std::string &folder_path) {
    Log::Debug("ToggleFolder: '%s' (IsSelected=%d, HasSelectionsUnder=%d)\n", 
               folder_path.c_str(), IsSelected(folder_path) ? 1 : 0, HasSelectionsUnder(folder_path) ? 1 : 0);
    
    if (IsSelected(folder_path)) {
        Log::Debug("ToggleFolder: Deselecting folder and contents\n");
        DeselectUnder(folder_path);
    } else if (HasSelectionsUnder(folder_path)) {
        Log::Debug("ToggleFolder: Completing partial selection\n");
        SelectRecursive(folder_path);
    } else {
        Log::Debug("ToggleFolder: Selecting folder and all contents\n");
        SelectRecursive(folder_path);
    }
    
    Log::Debug("ToggleFolder: Done. Total selected items: %zu\n", svc_->selected_paths.size());
}

void SelectionStore::SelectRecursive(const std::string &folder_path) {
    svc_->selected_paths.insert(folder_path);
    Log::Debug("SelectRecursive: Selected folder '%s'\n", folder_path.c_str());
    
    std::string device_part;
    std::string path_part;
    size_t colon_pos = folder_path.find(':');
    if (colon_pos != std::string::npos) {
        device_part = folder_path.substr(0, colon_pos + 1);
        path_part = folder_path.substr(colon_pos + 1);
    } else {
        path_part = folder_path;
    }
    
    std::vector<FsDirectoryEntry> entries;
    if (!FS::GetDirList(device_part, path_part, entries)) {
        Log::Debug("SelectRecursive: Failed to get directory list for '%s'\n", folder_path.c_str());
        UpdateCachedInfo();
        return;
    }
    
    Log::Debug("SelectRecursive: Found %zu entries in '%s'\n", entries.size(), folder_path.c_str());
    
    for (const auto &entry : entries) {
        if (std::strncmp(entry.name, "..", 2) == 0)
            continue;
        
        std::string child_path = folder_path;
        if (!child_path.empty() && child_path.back() != '/')
            child_path += "/";
        child_path += entry.name;
        
        svc_->selected_paths.insert(child_path);
        Log::Debug("SelectRecursive: Selected item '%s' (type=%d)\n", child_path.c_str(), entry.type);
        
        if (entry.type == FsDirEntryType_Dir) {
            SelectRecursive(child_path);
        }
    }
    
    UpdateCachedInfo();
}

void SelectionStore::DeselectUnder(const std::string &prefix) {
    std::string match_prefix = prefix;
    if (!match_prefix.empty() && match_prefix.back() != '/')
        match_prefix += "/";
    
    svc_->selected_paths.erase(prefix);
    
    auto it = svc_->selected_paths.lower_bound(match_prefix);
    while (it != svc_->selected_paths.end() && it->compare(0, match_prefix.length(), match_prefix) == 0) {
        it = svc_->selected_paths.erase(it);
    }
    
    DeselectAncestors(prefix);
    
    UpdateCachedInfo();
}

int SelectionStore::GetFolderSelectionState(const std::string &folder_path, const std::vector<FsDirectoryEntry> &entries) const {
    int total_children = 0;
    int selected_children = 0;
    
    std::string base_path = folder_path;
    if (!base_path.empty() && base_path.back() != '/')
        base_path += "/";
    
    for (const auto &entry : entries) {
        if (std::strncmp(entry.name, "..", 2) == 0)
            continue;
        
        total_children++;
        
        std::string child_path = base_path + entry.name;
        
        if (entry.type == FsDirEntryType_Dir) {
            if (IsSelected(child_path) || HasSelectionsUnder(child_path))
                selected_children++;
        } else {
            if (IsSelected(child_path))
                selected_children++;
        }
    }
    
    if (total_children == 0)
        return 0;
    
    if (selected_children == 0)
        return 0;
    else if (selected_children == total_children)
        return 2;
    else
        return 1;
}

void SelectionStore::Clear() {
    svc_->Clear();
}

void SelectionStore::SelectAll(const std::string &base_path, const std::vector<FsDirectoryEntry> &entries) {
    for (const auto &entry : entries) {
        if (std::strncmp(entry.name, "..", 2) == 0)
            continue;
        
        std::string full_path = base_path;
        if (!full_path.empty() && full_path.back() != '/')
            full_path += "/";
        full_path += entry.name;
        
        svc_->selected_paths.insert(full_path);
    }
    UpdateCachedInfo();
}

size_t SelectionStore::Count() const {
    return svc_->selected_paths.size();
}

bool SelectionStore::HasSelections() const {
    return !svc_->selected_paths.empty();
}

const std::string& SelectionStore::GetDevice() const {
    return svc_->device;
}

const std::string& SelectionStore::GetSelectionDirectory() const {
    return svc_->selection_directory;
}

const std::set<std::string>& SelectionStore::GetSelectedPaths() const {
    return svc_->selected_paths;
}

void SelectionStore::UpdateCachedInfo() {
    if (svc_->selected_paths.empty()) {
        svc_->device.clear();
        svc_->selection_directory.clear();
        return;
    }
    
    const std::string &first_path = *svc_->selected_paths.begin();
    size_t colon_pos = first_path.find(':');
    if (colon_pos != std::string::npos) {
        svc_->device = first_path.substr(0, colon_pos + 1);
    }
    
    svc_->selection_directory = GetParentPath(first_path);
    
    for (const auto &path : svc_->selected_paths) {
        std::string parent = GetParentPath(path);
        
        size_t i = 0;
        while (i < svc_->selection_directory.length() && i < parent.length() && 
               svc_->selection_directory[i] == parent[i]) {
            i++;
        }
        
        if (i < svc_->selection_directory.length()) {
            size_t last_slash = svc_->selection_directory.rfind('/', i);
            if (last_slash != std::string::npos)
                svc_->selection_directory = svc_->selection_directory.substr(0, last_slash);
            else
                svc_->selection_directory.clear();
        }
    }
}
