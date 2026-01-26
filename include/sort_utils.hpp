#pragma once

#include <cstring>
#include <switch.h>

// Sorting utility class to eliminate duplicate comparison code
// Provides common comparison primitives for file browser sorting
class SortUtils {
public:
    // Check if entry is the parent directory ".."
    static bool IsParentDir(const char* name) {
        return strcasecmp(name, "..") == 0;
    }

    static bool IsParentDir(const FsDirectoryEntry &entry) {
        return IsParentDir(entry.name);
    }

    // Case-insensitive alphabetical comparison
    // Returns: negative if a < b, 0 if equal, positive if a > b
    static int CompareAlpha(const char* a, const char* b) {
        return strcasecmp(a, b);
    }

    static int CompareAlpha(const FsDirectoryEntry &a, const FsDirectoryEntry &b) {
        return strcasecmp(a.name, b.name);
    }

    // Alphabetical sort result with direction support
    // Returns true if 'a' should come before 'b'
    static bool AlphaLess(const char* a, const char* b, bool descending = false) {
        return descending ? (strcasecmp(b, a) < 0) : (strcasecmp(a, b) < 0);
    }

    static bool AlphaLess(const FsDirectoryEntry &a, const FsDirectoryEntry &b, bool descending = false) {
        return AlphaLess(a.name, b.name, descending);
    }

    // Check if entry A is a directory and B is not (for directory-first ordering)
    static bool IsDirBeforeFile(const FsDirectoryEntry &a, const FsDirectoryEntry &b) {
        return (a.type == FsDirEntryType_Dir) && (b.type != FsDirEntryType_Dir);
    }

    // Check if entry is a directory
    static bool IsDirectory(const FsDirectoryEntry &entry) {
        return entry.type == FsDirEntryType_Dir;
    }

    // Combined parent dir priority check
    // Returns: 1 if A should come first (is ".."), -1 if B should come first (is ".."), 0 if neither
    static int ParentDirPriority(const FsDirectoryEntry &a, const FsDirectoryEntry &b) {
        if (IsParentDir(a)) return 1;   // A wins
        if (IsParentDir(b)) return -1;  // B wins
        return 0;  // Neither is parent
    }

    // Combined directory-first priority check
    // Returns: 1 if A should come first (dir vs file), -1 if B should come first (dir vs file), 0 if same type
    static int DirFirstPriority(const FsDirectoryEntry &a, const FsDirectoryEntry &b) {
        bool aIsDir = IsDirectory(a);
        bool bIsDir = IsDirectory(b);
        if (aIsDir && !bIsDir) return 1;   // A wins
        if (!aIsDir && bIsDir) return -1;  // B wins
        return 0;  // Same type
    }

    // Full standard ordering check: parent dir first, then directories, then alphabetical
    // Handles the common case used in simple non-table sorting
    // Returns true if 'a' should come before 'b'
    static bool StandardOrder(const FsDirectoryEntry &a, const FsDirectoryEntry &b, bool descending = false) {
        // Parent dir always first
        int parentPri = ParentDirPriority(a, b);
        if (parentPri != 0) return parentPri > 0;

        // Directories before files
        int dirPri = DirFirstPriority(a, b);
        if (dirPri != 0) return dirPri > 0;

        // Alphabetical
        return AlphaLess(a, b, descending);
    }

    // Comparison for numeric values with direction support
    // Returns true if 'a' should come before 'b'
    template<typename T>
    static bool NumericLess(T a, T b, bool descending = false) {
        return descending ? (a > b) : (a < b);
    }

    // Comparison for boolean values with direction support
    // By default, true values come first (e.g., checked items, archive bit set)
    // Returns true if 'a' should come before 'b'
    static bool BoolLess(bool a, bool b, bool descending = false) {
        // Default: true comes before false (a=true, b=false -> a first)
        // descending flips: false comes before true
        return descending ? (a < b) : (a > b);
    }
};
