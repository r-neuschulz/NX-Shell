// EPUB Reader implementation for NX-Shell
// Uses libarchive (existing) for ZIP reading, tinyxml2 for XML parsing,
// and litehtml for HTML/CSS rendering to ImGui.

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <memory>
#include <sstream>
#include <stack>

#include <archive.h>
#include <archive_entry.h>
#include <tinyxml2.h>
#include <litehtml.h>

#include "bottombar.hpp"
#include "config.hpp"
#include "epub.hpp"
#include "fs.hpp"
#include "gui.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "language.hpp"
#include "log.hpp"
#include "popups.hpp"
#include "services.hpp"
#include "windows.hpp"

// STB image for loading embedded EPUB images from memory
// We only need the declaration here (implementation is in textures.cpp)
// Use stbi_load_from_memory for in-memory image loading
extern "C" {
    extern unsigned char *stbi_load_from_memory(unsigned char const *buffer, int len, 
                                                 int *x, int *y, int *channels_in_file, 
                                                 int desired_channels);
    extern void stbi_image_free(void *retval_from_stbi_load);
}

// PNG loading from memory
#include <png.h>

// JPEG loading from memory
#include <turbojpeg.h>

// ============================================================================
// Minimal master CSS for litehtml (default HTML element styles)
// ============================================================================

static const char* MASTER_CSS = R"(
html { display: block; }
head { display: none; }
meta { display: none; }
title { display: none; }
link { display: none; }
style { display: none; }
script { display: none; }
body { display: block; margin: 8px; font-size: 16px; line-height: 1.4; }
p { display: block; margin-top: 1em; margin-bottom: 1em; }
div { display: block; }
span { display: inline; }
h1 { display: block; font-size: 2em; font-weight: bold; margin-top: 0.67em; margin-bottom: 0.67em; }
h2 { display: block; font-size: 1.5em; font-weight: bold; margin-top: 0.83em; margin-bottom: 0.83em; }
h3 { display: block; font-size: 1.17em; font-weight: bold; margin-top: 1em; margin-bottom: 1em; }
h4 { display: block; font-weight: bold; margin-top: 1.33em; margin-bottom: 1.33em; }
h5 { display: block; font-size: 0.83em; font-weight: bold; margin-top: 1.67em; margin-bottom: 1.67em; }
h6 { display: block; font-size: 0.67em; font-weight: bold; margin-top: 2.33em; margin-bottom: 2.33em; }
b, strong { font-weight: bold; display: inline; }
i, em { font-style: italic; display: inline; }
u { text-decoration: underline; display: inline; }
center { display: block; text-align: center; }
a { color: #0563C1; text-decoration: underline; display: inline; cursor: pointer; }
a:visited { color: #954F72; }
img { display: inline-block; }
br { display: inline; }
hr { display: block; margin-top: 0.5em; margin-bottom: 0.5em; border-top: 1px solid; }
ul { display: block; margin-top: 1em; margin-bottom: 1em; padding-left: 40px; list-style-type: disc; }
ol { display: block; margin-top: 1em; margin-bottom: 1em; padding-left: 40px; list-style-type: decimal; }
li { display: list-item; }
table { display: table; border-collapse: separate; border-spacing: 2px; }
tr { display: table-row; }
td, th { display: table-cell; padding: 1px; }
th { font-weight: bold; text-align: center; }
blockquote { display: block; margin: 1em 40px; }
pre { display: block; font-family: monospace; white-space: pre; margin: 1em 0; }
code { font-family: monospace; display: inline; }
sup { vertical-align: super; font-size: smaller; display: inline; }
sub { vertical-align: sub; font-size: smaller; display: inline; }
section { display: block; }
article { display: block; }
aside { display: block; }
header { display: block; }
footer { display: block; }
nav { display: block; }
figure { display: block; margin: 1em 40px; }
figcaption { display: block; }
)";

// ============================================================================
// Static state for the EPUB reader
// ============================================================================

static EpubBook s_book;
static std::string s_epub_path;
static std::map<std::string, EpubImage> s_image_cache;

// Rendered chapter content (litehtml document)
static std::shared_ptr<litehtml::document> s_current_doc;
static int s_content_height = 0;   // Total rendered content height
static float s_page_offset = 0.0f; // Current page vertical offset

// Scroll acceleration
static float s_scroll_hold_time = 0.0f;
static constexpr float SCROLL_ACCEL_RAMP_TIME = 1.0f;

// ============================================================================
// Helper: Read entire file from an EPUB (ZIP) archive into memory
// ============================================================================

static bool ReadEntryFromZip(const std::string &zip_path, const std::string &entry_path,
                             std::vector<unsigned char> &out_data) {
    struct archive *a = archive_read_new();
    if (!a) return false;
    
    archive_read_support_format_zip(a);
    archive_read_support_filter_none(a);
    
    if (archive_read_open_filename(a, zip_path.c_str(), 10240) != ARCHIVE_OK) {
        archive_read_free(a);
        return false;
    }
    
    struct archive_entry *entry;
    bool found = false;
    
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        const char *name = archive_entry_pathname(entry);
        if (!name) {
            archive_read_data_skip(a);
            continue;
        }
        
        // Normalize: remove leading ./ if present
        std::string normalized_name = name;
        if (normalized_name.substr(0, 2) == "./")
            normalized_name = normalized_name.substr(2);
        
        std::string normalized_target = entry_path;
        if (normalized_target.substr(0, 2) == "./")
            normalized_target = normalized_target.substr(2);
        
        if (normalized_name == normalized_target) {
            // Read the entry data
            la_int64_t entry_size = archive_entry_size(entry);
            if (entry_size > 0) {
                out_data.resize(static_cast<size_t>(entry_size));
                la_ssize_t bytes_read = archive_read_data(a, out_data.data(), out_data.size());
                if (bytes_read < 0) {
                    Log::Error("EpubParser: Failed to read entry %s: %s\n", 
                              entry_path.c_str(), archive_error_string(a));
                    out_data.clear();
                } else {
                    out_data.resize(static_cast<size_t>(bytes_read));
                    found = true;
                }
            } else {
                // Size unknown, read in chunks
                out_data.clear();
                const size_t chunk_size = 0x10000;
                std::vector<unsigned char> chunk(chunk_size);
                la_ssize_t bytes_read;
                while ((bytes_read = archive_read_data(a, chunk.data(), chunk_size)) > 0) {
                    out_data.insert(out_data.end(), chunk.begin(), chunk.begin() + bytes_read);
                }
                found = !out_data.empty();
            }
            break;
        }
        
        archive_read_data_skip(a);
    }
    
    archive_read_free(a);
    return found;
}

// ============================================================================
// Helper: Load all files from EPUB into memory cache
// ============================================================================

static void CacheAllFiles(const std::string &zip_path, EpubBook &book) {
    struct archive *a = archive_read_new();
    if (!a) return;
    
    archive_read_support_format_zip(a);
    archive_read_support_filter_none(a);
    
    if (archive_read_open_filename(a, zip_path.c_str(), 10240) != ARCHIVE_OK) {
        archive_read_free(a);
        return;
    }
    
    struct archive_entry *entry;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        const char *name = archive_entry_pathname(entry);
        if (!name || archive_entry_filetype(entry) == AE_IFDIR) {
            archive_read_data_skip(a);
            continue;
        }
        
        std::string path = name;
        if (path.substr(0, 2) == "./") path = path.substr(2);
        
        std::vector<unsigned char> data;
        la_int64_t entry_size = archive_entry_size(entry);
        
        if (entry_size > 0) {
            data.resize(static_cast<size_t>(entry_size));
            la_ssize_t bytes_read = archive_read_data(a, data.data(), data.size());
            if (bytes_read > 0) {
                data.resize(static_cast<size_t>(bytes_read));
                book.file_cache[path] = std::move(data);
            }
        } else {
            const size_t chunk_size = 0x10000;
            std::vector<unsigned char> chunk(chunk_size);
            la_ssize_t bytes_read;
            while ((bytes_read = archive_read_data(a, chunk.data(), chunk_size)) > 0) {
                data.insert(data.end(), chunk.begin(), chunk.begin() + bytes_read);
            }
            if (!data.empty()) {
                book.file_cache[path] = std::move(data);
            }
        }
    }
    
    archive_read_free(a);
}

// ============================================================================
// EpubParser implementation
// ============================================================================

namespace EpubParser {
    
    std::string ResolvePath(const EpubBook &book, const std::string &relative_path) {
        if (book.opf_dir.empty()) return relative_path;
        
        // Handle absolute paths (starting with /)
        if (!relative_path.empty() && relative_path[0] == '/') 
            return relative_path.substr(1);
        
        std::string result = book.opf_dir + "/" + relative_path;
        
        // Normalize: resolve ../ components
        std::filesystem::path p(result);
        result = p.lexically_normal().string();
        
        // Remove leading / if present
        if (!result.empty() && result[0] == '/') result = result.substr(1);
        // Remove leading ./ if present
        if (result.substr(0, 2) == "./") result = result.substr(2);
        
        return result;
    }
    
    bool Parse(const std::string &epub_path, EpubBook &book) {
        book = EpubBook{}; // Reset
        
        // Step 1: Cache all files from the archive for fast access
        CacheAllFiles(epub_path, book);
        
        // Step 2: Read META-INF/container.xml to find the OPF file
        std::vector<unsigned char> container_data;
        auto it = book.file_cache.find("META-INF/container.xml");
        if (it == book.file_cache.end()) {
            Log::Error("EpubParser: container.xml not found in EPUB\n");
            return false;
        }
        container_data = it->second;
        
        tinyxml2::XMLDocument container_doc;
        if (container_doc.Parse(reinterpret_cast<const char*>(container_data.data()), 
                                container_data.size()) != tinyxml2::XML_SUCCESS) {
            Log::Error("EpubParser: Failed to parse container.xml\n");
            return false;
        }
        
        // Find the rootfile element
        auto *container_el = container_doc.FirstChildElement("container");
        if (!container_el) {
            Log::Error("EpubParser: No <container> element\n");
            return false;
        }
        
        auto *rootfiles = container_el->FirstChildElement("rootfiles");
        if (!rootfiles) {
            Log::Error("EpubParser: No <rootfiles> element\n");
            return false;
        }
        
        auto *rootfile = rootfiles->FirstChildElement("rootfile");
        if (!rootfile) {
            Log::Error("EpubParser: No <rootfile> element\n");
            return false;
        }
        
        const char *opf_path = rootfile->Attribute("full-path");
        if (!opf_path) {
            Log::Error("EpubParser: rootfile has no full-path attribute\n");
            return false;
        }
        
        // Extract OPF directory
        std::string opf_path_str = opf_path;
        auto slash_pos = opf_path_str.rfind('/');
        book.opf_dir = (slash_pos != std::string::npos) ? opf_path_str.substr(0, slash_pos) : "";
        
        // Step 3: Read and parse the OPF file
        auto opf_it = book.file_cache.find(opf_path_str);
        if (opf_it == book.file_cache.end()) {
            Log::Error("EpubParser: OPF file not found: %s\n", opf_path);
            return false;
        }
        
        tinyxml2::XMLDocument opf_doc;
        if (opf_doc.Parse(reinterpret_cast<const char*>(opf_it->second.data()),
                          opf_it->second.size()) != tinyxml2::XML_SUCCESS) {
            Log::Error("EpubParser: Failed to parse OPF file\n");
            return false;
        }
        
        auto *package = opf_doc.FirstChildElement("package");
        if (!package) {
            // Try with namespace prefix
            package = opf_doc.FirstChildElement("opf:package");
        }
        if (!package) {
            Log::Error("EpubParser: No <package> element in OPF\n");
            return false;
        }
        
        // Step 4: Parse metadata
        auto *metadata = package->FirstChildElement("metadata");
        if (metadata) {
            auto *title_el = metadata->FirstChildElement("dc:title");
            if (!title_el) title_el = metadata->FirstChildElement("title");
            if (title_el && title_el->GetText())
                book.metadata.title = title_el->GetText();
            
            auto *creator_el = metadata->FirstChildElement("dc:creator");
            if (!creator_el) creator_el = metadata->FirstChildElement("creator");
            if (creator_el && creator_el->GetText())
                book.metadata.creator = creator_el->GetText();
            
            auto *lang_el = metadata->FirstChildElement("dc:language");
            if (!lang_el) lang_el = metadata->FirstChildElement("language");
            if (lang_el && lang_el->GetText())
                book.metadata.language = lang_el->GetText();
        }
        
        if (book.metadata.title.empty()) {
            // Fallback: use filename as title
            book.metadata.title = std::filesystem::path(epub_path).stem().string();
        }
        
        // Step 5: Parse manifest
        auto *manifest = package->FirstChildElement("manifest");
        if (!manifest) {
            Log::Error("EpubParser: No <manifest> element in OPF\n");
            return false;
        }
        
        for (auto *item = manifest->FirstChildElement("item"); item; item = item->NextSiblingElement("item")) {
            EpubManifestItem mi;
            if (item->Attribute("id")) mi.id = item->Attribute("id");
            if (item->Attribute("href")) mi.href = item->Attribute("href");
            if (item->Attribute("media-type")) mi.media_type = item->Attribute("media-type");
            
            if (!mi.id.empty() && !mi.href.empty()) {
                book.manifest.push_back(mi);
                book.manifest_by_id[mi.id] = mi;
                book.manifest_by_href[mi.href] = mi;
            }
        }
        
        // Step 6: Parse spine
        auto *spine = package->FirstChildElement("spine");
        if (!spine) {
            Log::Error("EpubParser: No <spine> element in OPF\n");
            return false;
        }
        
        for (auto *itemref = spine->FirstChildElement("itemref"); itemref; itemref = itemref->NextSiblingElement("itemref")) {
            EpubSpineItem si;
            if (itemref->Attribute("idref")) si.idref = itemref->Attribute("idref");
            if (itemref->Attribute("linear")) si.linear = itemref->Attribute("linear");
            
            if (!si.idref.empty()) {
                book.spine.push_back(si);
            }
        }
        
        Log::Debug("EpubParser: Loaded '%s' - %zu manifest items, %zu spine items\n",
                   book.metadata.title.c_str(), book.manifest.size(), book.spine.size());
        
        return !book.spine.empty();
    }
    
    bool ReadFileFromArchive(const std::string &epub_path, const std::string &internal_path,
                             std::vector<unsigned char> &out_data) {
        return ReadEntryFromZip(epub_path, internal_path, out_data);
    }
    
    std::string GetChapterContent(const EpubBook &book, int spine_index) {
        if (spine_index < 0 || spine_index >= static_cast<int>(book.spine.size()))
            return "";
        
        const EpubSpineItem &si = book.spine[spine_index];
        auto manifest_it = book.manifest_by_id.find(si.idref);
        if (manifest_it == book.manifest_by_id.end())
            return "";
        
        std::string resolved = ResolvePath(book, manifest_it->second.href);
        auto cache_it = book.file_cache.find(resolved);
        if (cache_it == book.file_cache.end())
            return "";
        
        return std::string(reinterpret_cast<const char*>(cache_it->second.data()),
                          cache_it->second.size());
    }
    
    std::string GetChapterCSS(const EpubBook &book, int spine_index) {
        // Get the chapter XHTML, then find CSS references within it
        std::string content = GetChapterContent(book, spine_index);
        if (content.empty()) return "";
        
        std::string combined_css;
        
        // Parse the XHTML to find <link rel="stylesheet"> and <style> elements
        tinyxml2::XMLDocument doc;
        if (doc.Parse(content.c_str(), content.size()) != tinyxml2::XML_SUCCESS)
            return "";
        
        // Find the <head> element and look for stylesheets
        std::function<void(tinyxml2::XMLElement*)> findCSS = [&](tinyxml2::XMLElement* el) {
            if (!el) return;
            
            const char* tag = el->Name();
            if (tag) {
                if (strcmp(tag, "link") == 0) {
                    const char* rel = el->Attribute("rel");
                    const char* href = el->Attribute("href");
                    if (rel && href && strcmp(rel, "stylesheet") == 0) {
                        // Get the spine item's directory for resolving relative CSS paths
                        std::string css_href = href;
                        const EpubSpineItem &si = book.spine[spine_index];
                        auto manifest_it = book.manifest_by_id.find(si.idref);
                        if (manifest_it != book.manifest_by_id.end()) {
                            // Resolve CSS path relative to the chapter file
                            std::string chapter_path = ResolvePath(book, manifest_it->second.href);
                            auto slash = chapter_path.rfind('/');
                            std::string chapter_dir = (slash != std::string::npos) ? chapter_path.substr(0, slash) : "";
                            
                            std::string css_path;
                            if (!chapter_dir.empty())
                                css_path = chapter_dir + "/" + css_href;
                            else
                                css_path = css_href;
                            
                            // Normalize path
                            std::filesystem::path p(css_path);
                            css_path = p.lexically_normal().string();
                            if (css_path.substr(0, 2) == "./") css_path = css_path.substr(2);
                            
                            auto css_it = book.file_cache.find(css_path);
                            if (css_it != book.file_cache.end()) {
                                combined_css += std::string(
                                    reinterpret_cast<const char*>(css_it->second.data()),
                                    css_it->second.size());
                                combined_css += "\n";
                            }
                        }
                    }
                }
                else if (strcmp(tag, "style") == 0) {
                    if (el->GetText()) {
                        combined_css += el->GetText();
                        combined_css += "\n";
                    }
                }
            }
            
            // Recurse into children
            for (auto *child = el->FirstChildElement(); child; child = child->NextSiblingElement()) {
                findCSS(child);
            }
        };
        
        findCSS(doc.RootElement());
        return combined_css;
    }
    
    int GetChapterCount(const EpubBook &book) {
        return static_cast<int>(book.spine.size());
    }
}

// ============================================================================
// Image loading helpers (from in-memory buffers)
// ============================================================================

static bool CreateGLTexture(unsigned char *pixels, int width, int height, GLuint &out_id) {
    if (!pixels || width <= 0 || height <= 0) return false;
    
    glGenTextures(1, &out_id);
    glBindTexture(GL_TEXTURE_2D, out_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    return true;
}

static bool LoadImageFromMemory(const std::vector<unsigned char> &data, EpubImage &img) {
    if (data.empty()) return false;
    
    // Try stb_image first (handles PNG, JPEG, GIF, BMP, PSD, TGA, PNM)
    int w = 0, h = 0;
    unsigned char *pixels = stbi_load_from_memory(data.data(), static_cast<int>(data.size()),
                                                   &w, &h, nullptr, 4);
    if (pixels) {
        img.width = w;
        img.height = h;
        bool ok = CreateGLTexture(pixels, w, h, img.texture_id);
        stbi_image_free(pixels);
        return ok;
    }
    
    // Try turbojpeg for JPEG files that stb might not handle well
    tjhandle jpeg = tjInitDecompress();
    if (jpeg) {
        int jpegsubsamp = 0;
        if (tjDecompressHeader2(jpeg, const_cast<unsigned char*>(data.data()), 
                                static_cast<unsigned long>(data.size()),
                                &w, &h, &jpegsubsamp) == 0 && w > 0 && h > 0) {
            std::vector<unsigned char> rgba(w * h * 4);
            if (tjDecompress2(jpeg, const_cast<unsigned char*>(data.data()),
                             static_cast<unsigned long>(data.size()),
                             rgba.data(), w, 0, h, TJPF_RGBA, TJFLAG_FASTDCT) == 0) {
                img.width = w;
                img.height = h;
                tjDestroy(jpeg);
                return CreateGLTexture(rgba.data(), w, h, img.texture_id);
            }
        }
        tjDestroy(jpeg);
    }
    
    return false;
}

static void FreeImageCache() {
    for (auto &pair : s_image_cache) {
        if (pair.second.texture_id) {
            glDeleteTextures(1, &pair.second.texture_id);
        }
    }
    s_image_cache.clear();
}

// ============================================================================
// ImGui LiteHTML Document Container
// ============================================================================

class ImGuiLiteHtmlContainer : public litehtml::document_container {
public:
    ImDrawList *m_draw_list = nullptr;
    float m_offset_x = 0.0f;    // Drawing offset X
    float m_offset_y = 0.0f;    // Drawing offset Y (for pagination)
    int m_client_width = 1280;
    int m_client_height = 675;   // display_height - bottom_bar
    bool m_dark_theme = true;
    const EpubBook *m_book = nullptr;
    std::string m_epub_path;
    
    // Clip rect stack
    std::stack<litehtml::position> m_clip_stack;
    
    // Font info (we use ImGui's default font, so this is simple)
    struct FontInfo {
        float size = 16.0f;
        int weight = 400;
        bool italic = false;
        unsigned int decoration = 0;
    };
    std::vector<FontInfo> m_fonts;  // Font pool (index = handle - 1)
    
    // ---- Font methods ----
    
    litehtml::uint_ptr create_font(const char* /*faceName*/, int size, int weight,
                                    litehtml::font_style italic, unsigned int decoration,
                                    litehtml::font_metrics* fm) override {
        FontInfo fi;
        fi.size = static_cast<float>(size);
        fi.weight = weight;
        fi.italic = (italic == litehtml::font_style_italic);
        fi.decoration = decoration;
        
        m_fonts.push_back(fi);
        litehtml::uint_ptr handle = static_cast<litehtml::uint_ptr>(m_fonts.size());
        
        // Fill in font metrics based on ImGui's default font
        if (fm) {
            float font_size = fi.size;
            if (font_size < 8.0f) font_size = 8.0f;
            
            fm->height = static_cast<int>(font_size * 1.2f);
            fm->ascent = static_cast<int>(font_size);
            fm->descent = static_cast<int>(font_size * 0.2f);
            fm->x_height = static_cast<int>(font_size * 0.5f);
            fm->draw_spaces = (italic == litehtml::font_style_italic);
        }
        
        return handle;
    }
    
    void delete_font(litehtml::uint_ptr /*hFont*/) override {
        // Fonts are stored in the pool, cleaned up when container is destroyed
    }
    
    int text_width(const char* text, litehtml::uint_ptr hFont) override {
        float font_size = 16.0f;
        if (hFont > 0 && hFont <= m_fonts.size()) {
            font_size = m_fonts[hFont - 1].size;
        }
        
        // Scale text width based on font size relative to ImGui's default
        float scale = font_size / ImGui::GetFontSize();
        ImVec2 sz = ImGui::CalcTextSize(text);
        return static_cast<int>(sz.x * scale);
    }
    
    void draw_text(litehtml::uint_ptr /*hDc*/, const char* text, litehtml::uint_ptr hFont,
                   litehtml::web_color color, const litehtml::position& pos) override {
        if (!m_draw_list || !text) return;
        
        float font_size = 16.0f;
        bool is_bold = false;
        unsigned int decoration = 0;
        if (hFont > 0 && hFont <= m_fonts.size()) {
            font_size = m_fonts[hFont - 1].size;
            is_bold = m_fonts[hFont - 1].weight >= 700;
            decoration = m_fonts[hFont - 1].decoration;
        }
        
        float x = static_cast<float>(pos.x) + m_offset_x;
        float y = static_cast<float>(pos.y) + m_offset_y;
        
        ImU32 col = IM_COL32(color.red, color.green, color.blue, color.alpha);
        
        // If default text color is black but we're in dark mode, use white
        if (color.red == 0 && color.green == 0 && color.blue == 0 && m_dark_theme) {
            col = IM_COL32(220, 220, 220, color.alpha);
        }
        
        float scale = font_size / ImGui::GetFontSize();
        
        // Draw text with scaling
        ImFont* font = ImGui::GetFont();
        m_draw_list->AddText(font, font_size, ImVec2(x, y), col, text);
        
        // Bold simulation: draw again with 1px offset
        if (is_bold) {
            m_draw_list->AddText(font, font_size, ImVec2(x + 1.0f, y), col, text);
        }
        
        // Underline decoration
        if (decoration & 1) { // litehtml::font_decoration_underline
            ImVec2 text_sz = ImGui::CalcTextSize(text);
            float text_w = text_sz.x * scale;
            float underline_y = y + font_size + 1.0f;
            m_draw_list->AddLine(ImVec2(x, underline_y), ImVec2(x + text_w, underline_y), col, 1.0f);
        }
        
        // Strikethrough decoration
        if (decoration & 2) { // litehtml::font_decoration_linethrough
            ImVec2 text_sz = ImGui::CalcTextSize(text);
            float text_w = text_sz.x * scale;
            float strike_y = y + font_size * 0.5f;
            m_draw_list->AddLine(ImVec2(x, strike_y), ImVec2(x + text_w, strike_y), col, 1.0f);
        }
    }
    
    int pt_to_px(int pt) const override {
        return static_cast<int>(pt * 96.0 / 72.0);
    }
    
    int get_default_font_size() const override {
        return 16;
    }
    
    const char* get_default_font_name() const override {
        return "sans-serif";
    }
    
    // ---- List marker ----
    
    void draw_list_marker(litehtml::uint_ptr /*hDc*/, const litehtml::list_marker& marker) override {
        if (!m_draw_list) return;
        
        float x = static_cast<float>(marker.pos.x) + m_offset_x;
        float y = static_cast<float>(marker.pos.y) + m_offset_y;
        float w = static_cast<float>(marker.pos.width);
        float h = static_cast<float>(marker.pos.height);
        
        ImU32 col = IM_COL32(marker.color.red, marker.color.green, marker.color.blue, marker.color.alpha);
        if (marker.color.red == 0 && marker.color.green == 0 && marker.color.blue == 0 && m_dark_theme) {
            col = IM_COL32(220, 220, 220, marker.color.alpha);
        }
        
        float cx = x + w * 0.5f;
        float cy = y + h * 0.5f;
        float radius = std::min(w, h) * 0.3f;
        
        if (marker.marker_type == litehtml::list_style_type_disc) {
            m_draw_list->AddCircleFilled(ImVec2(cx, cy), radius, col);
        } else if (marker.marker_type == litehtml::list_style_type_circle) {
            m_draw_list->AddCircle(ImVec2(cx, cy), radius, col, 0, 1.5f);
        } else if (marker.marker_type == litehtml::list_style_type_square) {
            m_draw_list->AddRectFilled(ImVec2(cx - radius, cy - radius), 
                                        ImVec2(cx + radius, cy + radius), col);
        }
        // For numbered lists, the number text is drawn by draw_text
    }
    
    // ---- Image methods ----
    
    void load_image(const char* src, const char* /*baseurl*/, bool /*redraw_on_ready*/) override {
        if (!src || !m_book) return;
        
        std::string src_str = src;
        
        // Already cached?
        if (s_image_cache.find(src_str) != s_image_cache.end()) return;
        
        // Resolve path relative to EPUB
        std::string resolved = EpubParser::ResolvePath(*m_book, src_str);
        auto it = m_book->file_cache.find(resolved);
        if (it == m_book->file_cache.end()) {
            Log::Debug("EpubReader: Image not found in EPUB: %s (resolved: %s)\n", src, resolved.c_str());
            return;
        }
        
        EpubImage img;
        if (LoadImageFromMemory(it->second, img)) {
            s_image_cache[src_str] = img;
            Log::Debug("EpubReader: Loaded image %s (%dx%d)\n", src, img.width, img.height);
        }
    }
    
    void get_image_size(const char* src, const char* /*baseurl*/, litehtml::size& sz) override {
        if (!src) { sz.width = 0; sz.height = 0; return; }
        
        auto it = s_image_cache.find(std::string(src));
        if (it != s_image_cache.end()) {
            sz.width = it->second.width;
            sz.height = it->second.height;
        } else {
            sz.width = 0;
            sz.height = 0;
        }
    }
    
    // ---- Background and borders ----
    
    void draw_background(litehtml::uint_ptr /*hDc*/, const std::vector<litehtml::background_paint>& bg) override {
        if (!m_draw_list) return;
        
        for (const auto &b : bg) {
            // Draw solid background color
            if (b.color.alpha > 0) {
                // Skip white backgrounds in dark mode (they look bad)
                if (m_dark_theme && b.color.red > 240 && b.color.green > 240 && b.color.blue > 240)
                    continue;
                
                ImU32 col = IM_COL32(b.color.red, b.color.green, b.color.blue, b.color.alpha);
                float x = static_cast<float>(b.clip_box.x) + m_offset_x;
                float y = static_cast<float>(b.clip_box.y) + m_offset_y;
                float w = static_cast<float>(b.clip_box.width);
                float h = static_cast<float>(b.clip_box.height);
                m_draw_list->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), col);
            }
            
            // Draw background image
            if (!b.image.empty()) {
                auto it = s_image_cache.find(b.image);
                if (it != s_image_cache.end() && it->second.texture_id) {
                    float x = static_cast<float>(b.image_size.width > 0 ? b.position_x : b.clip_box.x) + m_offset_x;
                    float y = static_cast<float>(b.image_size.height > 0 ? b.position_y : b.clip_box.y) + m_offset_y;
                    float w = static_cast<float>(b.image_size.width > 0 ? b.image_size.width : it->second.width);
                    float h = static_cast<float>(b.image_size.height > 0 ? b.image_size.height : it->second.height);
                    
                    m_draw_list->AddImage(
                        static_cast<ImTextureID>(it->second.texture_id),
                        ImVec2(x, y), ImVec2(x + w, y + h));
                }
            }
        }
    }
    
    void draw_borders(litehtml::uint_ptr /*hDc*/, const litehtml::borders& borders,
                      const litehtml::position& draw_pos, bool /*root*/) override {
        if (!m_draw_list) return;
        
        float x = static_cast<float>(draw_pos.x) + m_offset_x;
        float y = static_cast<float>(draw_pos.y) + m_offset_y;
        float w = static_cast<float>(draw_pos.width);
        float h = static_cast<float>(draw_pos.height);
        
        auto draw_border = [&](const litehtml::border& b, ImVec2 p1, ImVec2 p2) {
            if (b.width > 0 && b.style != litehtml::border_style_none && b.style != litehtml::border_style_hidden) {
                ImU32 col = IM_COL32(b.color.red, b.color.green, b.color.blue, b.color.alpha);
                if (b.color.red == 0 && b.color.green == 0 && b.color.blue == 0 && m_dark_theme) {
                    col = IM_COL32(150, 150, 150, b.color.alpha);
                }
                m_draw_list->AddLine(p1, p2, col, static_cast<float>(b.width));
            }
        };
        
        // Top
        draw_border(borders.top, ImVec2(x, y), ImVec2(x + w, y));
        // Bottom
        draw_border(borders.bottom, ImVec2(x, y + h), ImVec2(x + w, y + h));
        // Left
        draw_border(borders.left, ImVec2(x, y), ImVec2(x, y + h));
        // Right
        draw_border(borders.right, ImVec2(x + w, y), ImVec2(x + w, y + h));
    }
    
    // ---- Misc callbacks ----
    
    void set_caption(const char* /*caption*/) override {}
    void set_base_url(const char* /*base_url*/) override {}
    
    void link(const std::shared_ptr<litehtml::document>& /*doc*/, const litehtml::element::ptr& /*el*/) override {}
    void on_anchor_click(const char* /*url*/, const litehtml::element::ptr& /*el*/) override {}
    void set_cursor(const char* /*cursor*/) override {}
    
    void transform_text(litehtml::string& text, litehtml::text_transform tt) override {
        if (tt == litehtml::text_transform_uppercase) {
            std::transform(text.begin(), text.end(), text.begin(), ::toupper);
        } else if (tt == litehtml::text_transform_lowercase) {
            std::transform(text.begin(), text.end(), text.begin(), ::tolower);
        } else if (tt == litehtml::text_transform_capitalize && !text.empty()) {
            text[0] = static_cast<char>(::toupper(text[0]));
        }
    }
    
    void import_css(litehtml::string& text, const litehtml::string& url, litehtml::string& /*baseurl*/) override {
        if (!m_book) return;
        
        std::string resolved = EpubParser::ResolvePath(*m_book, url);
        auto it = m_book->file_cache.find(resolved);
        if (it != m_book->file_cache.end()) {
            text = std::string(reinterpret_cast<const char*>(it->second.data()), it->second.size());
        }
    }
    
    void set_clip(const litehtml::position& pos, const litehtml::border_radiuses& /*bdr_radius*/) override {
        m_clip_stack.push(pos);
        if (m_draw_list) {
            float x = static_cast<float>(pos.x) + m_offset_x;
            float y = static_cast<float>(pos.y) + m_offset_y;
            m_draw_list->PushClipRect(ImVec2(x, y), 
                                       ImVec2(x + static_cast<float>(pos.width), 
                                              y + static_cast<float>(pos.height)), true);
        }
    }
    
    void del_clip() override {
        if (!m_clip_stack.empty()) {
            m_clip_stack.pop();
            if (m_draw_list) {
                m_draw_list->PopClipRect();
            }
        }
    }
    
    void get_client_rect(litehtml::position& client) const override {
        client.x = 0;
        client.y = 0;
        client.width = m_client_width;
        client.height = m_client_height;
    }
    
    std::shared_ptr<litehtml::element> create_element(const char* /*tag_name*/, 
                                                       const litehtml::string_map& /*attributes*/,
                                                       const std::shared_ptr<litehtml::document>& /*doc*/) override {
        return nullptr; // Use default elements
    }
    
    void get_media_features(litehtml::media_features& media) const override {
        media.type = litehtml::media_type_screen;
        media.width = m_client_width;
        media.height = m_client_height;
        media.device_width = m_client_width;
        media.device_height = m_client_height;
        media.color = 8;
        media.monochrome = 0;
        media.color_index = 256;
        media.resolution = 96;
    }
    
    void get_language(litehtml::string& language, litehtml::string& culture) const override {
        language = "en";
        culture = "";
    }
};

// ============================================================================
// Static container instance
// ============================================================================

static ImGuiLiteHtmlContainer s_container;

// ============================================================================
// Chapter rendering
// ============================================================================

static bool RenderChapter(App &app, int spine_index) {
    std::string html_content = EpubParser::GetChapterContent(s_book, spine_index);
    if (html_content.empty()) {
        Log::Error("EpubReader: Empty chapter content at spine index %d\n", spine_index);
        return false;
    }
    
    // Get chapter CSS
    std::string chapter_css = EpubParser::GetChapterCSS(s_book, spine_index);
    
    // Combine master CSS + chapter CSS
    std::string combined_css = std::string(MASTER_CSS) + "\n" + chapter_css;
    
    // Set up container
    s_container.m_book = &s_book;
    s_container.m_epub_path = s_epub_path;
    s_container.m_dark_theme = GUI::IsCurrentThemeDark(app.config);
    
    // Calculate client dimensions
    float bottom_bar_height = 45.0f;
    int display_w = app.gui.display_width;
    int display_h = app.gui.display_height;
    
    int page_width = display_w;
    if (app.window.epub_side_by_side)
        page_width = display_w / 2 - 10;  // Half width minus divider padding
    
    s_container.m_client_width = page_width - 20;  // Padding
    s_container.m_client_height = display_h - static_cast<int>(bottom_bar_height);
    
    // Create the litehtml document
    s_current_doc = litehtml::document::createFromString(html_content.c_str(), &s_container, combined_css.c_str());
    
    if (!s_current_doc) {
        Log::Error("EpubReader: Failed to create litehtml document\n");
        return false;
    }
    
    // Render (layout) the document
    s_current_doc->render(s_container.m_client_width);
    s_content_height = s_current_doc->height();
    
    // Calculate pagination
    float page_height = static_cast<float>(s_container.m_client_height);
    app.window.epub_total_pages = std::max(1, static_cast<int>(std::ceil(
        static_cast<float>(s_content_height) / page_height)));
    app.window.epub_current_page = 0;
    s_page_offset = 0.0f;
    
    Log::Debug("EpubReader: Chapter %d rendered - content height: %d, pages: %d\n",
              spine_index, s_content_height, app.window.epub_total_pages);
    
    return true;
}

// ============================================================================
// EpubReader namespace implementation
// ============================================================================

namespace EpubReader {
    
    bool LoadFile(App &app, const std::string &path) {
        // Clean up any previous state
        Clear(app);
        
        s_epub_path = path;
        
        // Parse the EPUB structure
        if (!EpubParser::Parse(path, s_book)) {
            Log::Error("EpubReader: Failed to parse EPUB: %s\n", path.c_str());
            Toast::Show("Failed to open EPUB", false, 3.0f);
            return false;
        }
        
        // Reset state
        app.window.epub_chapter_index = 0;
        app.window.epub_current_page = 0;
        app.window.epub_total_pages = 0;
        
        // Render the first chapter
        if (!RenderChapter(app, 0)) {
            Log::Error("EpubReader: Failed to render first chapter\n");
            Toast::Show("Failed to render EPUB", false, 3.0f);
            return false;
        }
        
        return true;
    }
    
    void Clear(App &app) {
        s_current_doc.reset();
        s_book = EpubBook{};
        s_epub_path.clear();
        FreeImageCache();
        s_container.m_fonts.clear();
        s_content_height = 0;
        s_page_offset = 0.0f;
        
        app.window.epub_chapter_index = 0;
        app.window.epub_current_page = 0;
        app.window.epub_total_pages = 0;
    }
    
    bool HandleScroll(App &app, int index) {
        if (app.window.entries[index].type == FsDirEntryType_Dir) return false;
        
        FileType type = FS::GetFileType(app.window.entries[index].name);
        if (type != FileTypeEpub) return false;
        
        app.window.selected = index;
        std::string path = FS::BuildPath(app.fs, app.window.entries[index]);
        return LoadFile(app, path);
    }
    
    void ToggleViewMode(App &app) {
        app.window.epub_side_by_side = !app.window.epub_side_by_side;
        
        // Re-render with new dimensions
        if (s_current_doc) {
            RenderChapter(app, app.window.epub_chapter_index);
        }
    }
    
    void HandleControls(App &app, u64 &key, bool &properties) {
        if (key & HidNpadButton_X)
            properties = true;
        
        if (!properties) {
            // Y button toggles fullscreen / side-by-side
            if (key & HidNpadButton_Y) {
                ToggleViewMode(app);
            }
            
            // L/R buttons: previous/next chapter
            if (key & HidNpadButton_L) {
                int new_chapter = app.window.epub_chapter_index - 1;
                if (new_chapter >= 0) {
                    FreeImageCache();
                    s_container.m_fonts.clear();
                    app.window.epub_chapter_index = new_chapter;
                    RenderChapter(app, new_chapter);
                }
            }
            
            if (key & HidNpadButton_R) {
                int new_chapter = app.window.epub_chapter_index + 1;
                if (new_chapter < EpubParser::GetChapterCount(s_book)) {
                    FreeImageCache();
                    s_container.m_fonts.clear();
                    app.window.epub_chapter_index = new_chapter;
                    RenderChapter(app, new_chapter);
                }
            }
            
            // DPad Left/Right: previous/next page
            if (key & HidNpadButton_Left) {
                if (app.window.epub_current_page > 0) {
                    app.window.epub_current_page--;
                    float page_height = static_cast<float>(s_container.m_client_height);
                    s_page_offset = static_cast<float>(app.window.epub_current_page) * page_height;
                } else if (app.window.epub_chapter_index > 0) {
                    // Go to previous chapter, last page
                    FreeImageCache();
                    s_container.m_fonts.clear();
                    app.window.epub_chapter_index--;
                    RenderChapter(app, app.window.epub_chapter_index);
                    // Jump to last page
                    app.window.epub_current_page = app.window.epub_total_pages - 1;
                    float page_height = static_cast<float>(s_container.m_client_height);
                    s_page_offset = static_cast<float>(app.window.epub_current_page) * page_height;
                }
            }
            
            if (key & HidNpadButton_Right) {
                int step = app.window.epub_side_by_side ? 2 : 1;
                if (app.window.epub_current_page + step < app.window.epub_total_pages) {
                    app.window.epub_current_page += step;
                    float page_height = static_cast<float>(s_container.m_client_height);
                    s_page_offset = static_cast<float>(app.window.epub_current_page) * page_height;
                } else if (app.window.epub_chapter_index + 1 < EpubParser::GetChapterCount(s_book)) {
                    // Go to next chapter
                    FreeImageCache();
                    s_container.m_fonts.clear();
                    app.window.epub_chapter_index++;
                    RenderChapter(app, app.window.epub_chapter_index);
                }
            }
            
            // Right stick scrolling (smooth, within page)
            const float dt = 1.0f / 60.0f;
            const float stick_base_speed = 120.0f;
            const float stick_max_speed = 1200.0f;
            
            bool scroll_active = (key & (HidNpadButton_StickRUp | HidNpadButton_StickRDown)) != 0;
            if (scroll_active) {
                s_scroll_hold_time += dt;
                if (s_scroll_hold_time > SCROLL_ACCEL_RAMP_TIME)
                    s_scroll_hold_time = SCROLL_ACCEL_RAMP_TIME;
            } else {
                s_scroll_hold_time = 0.0f;
            }
            
            float accel = s_scroll_hold_time / SCROLL_ACCEL_RAMP_TIME;
            accel = accel * accel;
            float speed = stick_base_speed + (stick_max_speed - stick_base_speed) * accel;
            
            if (key & HidNpadButton_StickRUp) {
                s_page_offset -= speed;
                if (s_page_offset < 0.0f) s_page_offset = 0.0f;
                // Update current page from scroll offset
                float page_height = static_cast<float>(s_container.m_client_height);
                app.window.epub_current_page = static_cast<int>(s_page_offset / page_height);
            }
            if (key & HidNpadButton_StickRDown) {
                float max_offset = static_cast<float>(s_content_height - s_container.m_client_height);
                if (max_offset < 0) max_offset = 0;
                s_page_offset += speed;
                if (s_page_offset > max_offset) s_page_offset = max_offset;
                float page_height = static_cast<float>(s_container.m_client_height);
                app.window.epub_current_page = static_cast<int>(s_page_offset / page_height);
            }
        }
    }
}

// ============================================================================
// Windows::EpubReaderWindow - render the EPUB reader UI
// ============================================================================

namespace Windows {
    
    static void DrawEpubReaderBottomBar(App &app) {
        const int lang = app.config.Lang();
        
        // Left-aligned items
        std::vector<BottomBar::HintItem> left_items = {
            {BottomBar::ButtonType::Minus, strings[lang][Lang::HintExit]}
        };
        
        // Right-aligned items
        std::vector<BottomBar::HintItem> right_items = {
            {BottomBar::ButtonType::CircleB, strings[lang][Lang::HintBack]},
            {BottomBar::ButtonType::CircleY, app.window.epub_side_by_side ? "Fullscreen" : "Side-by-side",
             app.window.epub_side_by_side},
            {BottomBar::ButtonType::ShoulderL, strings[lang][Lang::HintPrev]},
            {BottomBar::ButtonType::ShoulderR, strings[lang][Lang::HintNext]},
            {BottomBar::ButtonType::RightStick, "Scroll"}
        };
        
        BottomBar::Config config;
        config.use_foreground_draw_list = true;
        config.draw_background = true;
        
        BottomBar::Draw(app.config, config, left_items, right_items);
    }
    
    void EpubReaderWindow(App &app, bool &properties, bool &file_stat) {
        const float display_w = static_cast<float>(app.gui.display_width);
        const float display_h = static_cast<float>(app.gui.display_height);
        const float bottom_bar_height = 45.0f;
        const float window_h = display_h - bottom_bar_height;
        
        // Set up fullscreen window
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(display_w, window_h), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        
        if (ImGui::Begin("##EpubReader", nullptr,
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | 
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
            
            ImDrawList *draw_list = ImGui::GetWindowDrawList();
            
            if (s_current_doc && draw_list) {
                float content_padding = 10.0f;
                
                if (app.window.epub_side_by_side) {
                    // Side-by-side mode: two pages
                    float half_w = display_w / 2.0f;
                    float divider_w = 2.0f;
                    
                    // Draw left page
                    draw_list->PushClipRect(ImVec2(0, 0), ImVec2(half_w - divider_w, window_h), true);
                    s_container.m_draw_list = draw_list;
                    s_container.m_offset_x = content_padding;
                    s_container.m_offset_y = -s_page_offset;
                    
                    litehtml::position clip_left;
                    clip_left.x = 0;
                    clip_left.y = static_cast<int>(s_page_offset);
                    clip_left.width = static_cast<int>(half_w - divider_w - content_padding * 2);
                    clip_left.height = static_cast<int>(window_h);
                    s_current_doc->draw(reinterpret_cast<litehtml::uint_ptr>(draw_list), 0, 0, &clip_left);
                    draw_list->PopClipRect();
                    
                    // Draw vertical divider
                    ImU32 divider_color = GUI::IsCurrentThemeDark(app.config) 
                        ? IM_COL32(80, 80, 80, 255) : IM_COL32(180, 180, 180, 255);
                    draw_list->AddLine(ImVec2(half_w, 0), ImVec2(half_w, window_h), divider_color, divider_w);
                    
                    // Draw right page (next page offset)
                    float right_page_offset = s_page_offset + window_h;
                    if (right_page_offset < static_cast<float>(s_content_height)) {
                        draw_list->PushClipRect(ImVec2(half_w + divider_w, 0), 
                                                ImVec2(display_w, window_h), true);
                        s_container.m_offset_x = half_w + divider_w + content_padding;
                        s_container.m_offset_y = -right_page_offset;
                        
                        litehtml::position clip_right;
                        clip_right.x = 0;
                        clip_right.y = static_cast<int>(right_page_offset);
                        clip_right.width = static_cast<int>(half_w - divider_w - content_padding * 2);
                        clip_right.height = static_cast<int>(window_h);
                        s_current_doc->draw(reinterpret_cast<litehtml::uint_ptr>(draw_list), 0, 0, &clip_right);
                        draw_list->PopClipRect();
                    }
                } else {
                    // Fullscreen mode: single page
                    draw_list->PushClipRect(ImVec2(0, 0), ImVec2(display_w, window_h), true);
                    s_container.m_draw_list = draw_list;
                    s_container.m_offset_x = content_padding;
                    s_container.m_offset_y = -s_page_offset;
                    
                    litehtml::position clip;
                    clip.x = 0;
                    clip.y = static_cast<int>(s_page_offset);
                    clip.width = static_cast<int>(display_w - content_padding * 2);
                    clip.height = static_cast<int>(window_h);
                    s_current_doc->draw(reinterpret_cast<litehtml::uint_ptr>(draw_list), 0, 0, &clip);
                    draw_list->PopClipRect();
                }
                
                // Draw page indicator (bottom-right, above the bottom bar)
                char page_info[64];
                int total_chapters = EpubParser::GetChapterCount(s_book);
                std::snprintf(page_info, sizeof(page_info), "Ch %d/%d  Page %d/%d",
                             app.window.epub_chapter_index + 1, total_chapters,
                             app.window.epub_current_page + 1, app.window.epub_total_pages);
                
                ImVec2 text_size = ImGui::CalcTextSize(page_info);
                ImU32 text_color = GUI::IsCurrentThemeDark(app.config) 
                    ? IM_COL32(180, 180, 180, 200) : IM_COL32(100, 100, 100, 200);
                
                ImGui::GetForegroundDrawList()->AddText(
                    ImVec2(display_w - text_size.x - 15.0f, window_h - text_size.y - 8.0f),
                    text_color, page_info);
            } else {
                // No content rendered - show message
                ImGui::TextUnformatted("Loading...");
            }
        }
        ImGui::End();
        ImGui::PopStyleVar();
        
        // Draw the bottom bar
        DrawEpubReaderBottomBar(app);
        
        // Draw book title as filename toast overlay
        if (!s_book.metadata.title.empty()) {
            Toast::DrawFilename(app.config, s_book.metadata.title.c_str());
        }
        
        if (properties)
            Popups::FilePropertiesPopup(app, file_stat, &properties);
    }
}
