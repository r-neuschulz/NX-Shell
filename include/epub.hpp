#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <glad/glad.h>

#include "services.hpp"

// Forward declarations (avoid pulling in heavy litehtml headers here)
namespace litehtml {
    class document;
    class document_container;
}

// ============================================================================
// EPUB Parser - reads and parses EPUB structure from ZIP archive
// ============================================================================

struct EpubManifestItem {
    std::string id;
    std::string href;         // Relative path within EPUB
    std::string media_type;
};

struct EpubSpineItem {
    std::string idref;        // References manifest item id
    std::string linear;       // "yes" or "no"
};

struct EpubMetadata {
    std::string title;
    std::string creator;
    std::string language;
};

struct EpubBook {
    EpubMetadata metadata;
    std::vector<EpubManifestItem> manifest;
    std::map<std::string, EpubManifestItem> manifest_by_id;   // id -> item
    std::map<std::string, EpubManifestItem> manifest_by_href; // href -> item
    std::vector<EpubSpineItem> spine;
    std::string opf_dir;      // Directory containing the OPF file (for resolving relative paths)
    
    // In-memory file cache (path -> data)
    std::map<std::string, std::vector<unsigned char>> file_cache;
};

// ============================================================================
// Image cache for EPUB embedded images
// ============================================================================

struct EpubImage {
    GLuint texture_id = 0;
    int width = 0;
    int height = 0;
};

// ============================================================================
// EPUB Parser namespace
// ============================================================================

namespace EpubParser {
    // Parse an EPUB file and populate the EpubBook structure
    // Returns true on success, false on failure
    bool Parse(const std::string &epub_path, EpubBook &book);
    
    // Read a file from the EPUB archive into an in-memory buffer
    // The result is cached in book.file_cache
    bool ReadFileFromArchive(const std::string &epub_path, const std::string &internal_path, 
                             std::vector<unsigned char> &out_data);
    
    // Get the XHTML content for a spine item (chapter)
    // Resolves the spine item -> manifest item -> file content
    std::string GetChapterContent(const EpubBook &book, int spine_index);
    
    // Get the CSS content referenced in the chapter
    std::string GetChapterCSS(const EpubBook &book, int spine_index);
    
    // Get the number of chapters (spine items)
    int GetChapterCount(const EpubBook &book);
    
    // Resolve a relative path against the OPF directory
    std::string ResolvePath(const EpubBook &book, const std::string &relative_path);
}
