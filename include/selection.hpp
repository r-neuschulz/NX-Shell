#pragma once

#include <set>
#include <string>
#include <vector>
#include <switch.h>

// Centralized selection storage - stores selected items by their full paths
// This decouples selection state from directory navigation
class SelectionStore {
public:
    // Check if a specific item is selected (exact match)
    // path should be the full path: device + cwd + "/" + name
    bool IsSelected(const std::string &path) const;
    
    // Check if any selections exist under a given directory prefix
    // Used to show partial-check icon on folders containing selections
    bool HasSelectionsUnder(const std::string &prefix) const;
    
    // Check if we're inside a selected folder
    // Returns true if any ancestor directory is selected
    bool IsInsideSelectedFolder(const std::string &path) const;
    
    // Select an item by its full path
    void Select(const std::string &path);
    
    // Deselect an item by its full path
    void Deselect(const std::string &path);
    
    // Toggle selection state (for files only - folders should use ToggleFolder)
    void Toggle(const std::string &path);
    
    // Toggle folder selection - recursively selects/deselects all contents
    void ToggleFolder(const std::string &folder_path);
    
    // Recursively select all files and folders under a directory
    // This explicitly adds every item so users can later unselect individual items
    void SelectRecursive(const std::string &folder_path);
    
    // Deselect all items under a directory prefix (including the prefix itself)
    void DeselectUnder(const std::string &prefix);
    
    // Check if all direct children in entries are selected
    // Returns: 0 = none selected, 1 = some selected (partial), 2 = all selected
    int GetFolderSelectionState(const std::string &folder_path, const std::vector<FsDirectoryEntry> &entries) const;
    
    // Clear all selections
    void Clear();
    
    // Select all items in a directory (given entries list and base path)
    void SelectAll(const std::string &base_path, const std::vector<FsDirectoryEntry> &entries);
    
    // Get count of selected items
    size_t Count() const;
    
    // Check if any selections exist
    bool HasSelections() const;
    
    // Get the device where selections exist (empty if no selections)
    const std::string& GetDevice() const;
    
    // Get the directory containing the selections (for single-directory constraint)
    // Returns empty string if selections span multiple directories
    const std::string& GetSelectionDirectory() const;
    
    // Get all selected paths (for iteration in copy/move/delete operations)
    const std::set<std::string>& GetSelectedPaths() const;
    
    // Build full path from device, cwd and entry name
    static std::string BuildFullPath(const std::string &device, const std::string &cwd, const std::string &name);
    
    // Extract just the filename from a full path
    static std::string GetFilename(const std::string &full_path);
    
    // Extract the parent directory from a full path
    static std::string GetParentPath(const std::string &full_path);

private:
    std::set<std::string> selected_paths_;  // Set of fully qualified paths (device:path)
    std::string device_;                     // Device where selections exist
    std::string selection_directory_;        // Base directory of selections
    
    // Update cached device/directory when selections change
    void UpdateCachedInfo();
    
    // Remove all ancestor paths when deselecting an item
    // This ensures parent folders show partcheck instead of check
    void DeselectAncestors(const std::string &path);
};

// Global selection store instance
extern SelectionStore g_selection;
