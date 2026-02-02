#include <cstring>
#include <filesystem>
#include <memory>
#include <archive.h>
#include <archive_entry.h>

#include "archive.hpp"
#include "config.hpp"
#include "fs.hpp"
#include "gui.hpp"
#include "services.hpp"
#include "imgui.h"
#include "language.hpp"
#include "log.hpp"
#include "popups.hpp"
#include "selection.hpp"
#include "windows.hpp"

namespace Archive {
    static std::string archive_path;
    static std::string extract_dest;
    
    void SetArchivePath(FileSystemService &fs_svc, const std::string &path) {
        archive_path = path;
        // Default extraction destination: same directory as the archive
        std::filesystem::path p(path);
        extract_dest = p.parent_path().string();
        if (extract_dest.empty()) {
            extract_dest = fs_svc.device + fs_svc.cwd;
        }
    }
    
    const std::string& GetArchivePath(void) {
        return archive_path;
    }
    
    // Count entries in archive for progress reporting
    static int64_t CountArchiveEntries(const std::string &path) {
        struct archive *a = archive_read_new();
        if (!a) return -1;
        
        archive_read_support_format_all(a);
        archive_read_support_filter_all(a);
        
        if (archive_read_open_filename(a, path.c_str(), 10240) != ARCHIVE_OK) {
            archive_read_free(a);
            return -1;
        }
        
        int64_t count = 0;
        struct archive_entry *entry;
        while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
            count++;
            archive_read_data_skip(a);
        }
        
        archive_read_free(a);
        return count;
    }
    
    // Get the base name for extraction directory
    // Handles compound extensions like .tar.gz, .tar.xz, etc.
    static std::string GetArchiveBaseName(const std::string &path) {
        std::filesystem::path p(path);
        std::string name = p.filename().string();
        
        // Handle compound extensions (.tar.gz, .tar.bz2, .tar.xz, .tar.lzma)
        const char* compound_exts[] = {".tar.gz", ".tar.bz2", ".tar.xz", ".tar.lzma", ".tgz", ".tbz2", ".txz"};
        std::string name_lower = name;
        for (auto &c : name_lower) c = std::tolower(c);
        
        for (const char* ext : compound_exts) {
            size_t ext_len = strlen(ext);
            if (name_lower.length() > ext_len && 
                name_lower.substr(name_lower.length() - ext_len) == ext) {
                return name.substr(0, name.length() - ext_len);
            }
        }
        
        // Standard single extension
        return p.stem().string();
    }
    
    bool Extract(App &app) {
        if (archive_path.empty()) {
            Log::Error("Archive::Extract - No archive path set\n");
            return false;
        }
        
        // Count total entries for progress
        int64_t total_entries = CountArchiveEntries(archive_path);
        if (total_entries < 0) {
            Log::Error("Archive::Extract - Failed to count archive entries: %s\n", archive_path.c_str());
            // Continue anyway, progress will be indeterminate
            total_entries = 0;
        }
        
        // Open archive for extraction
        struct archive *a = archive_read_new();
        if (!a) {
            Log::Error("Archive::Extract - Failed to create archive reader\n");
            return false;
        }
        
        // Enable all supported formats and filters
        archive_read_support_format_all(a);
        archive_read_support_filter_all(a);
        
        // Open the archive file
        if (archive_read_open_filename(a, archive_path.c_str(), 10240) != ARCHIVE_OK) {
            Log::Error("Archive::Extract - Failed to open archive: %s (%s)\n", 
                      archive_path.c_str(), archive_error_string(a));
            archive_read_free(a);
            return false;
        }
        
        const int lang = app.config.Lang();
        const std::string title = strings[lang][Lang::ArchiveTitle];
        const std::string extracting_prefix = strings[lang][Lang::ArchiveExtracting];
        
        // Create base extraction directory (archive name without extension)
        std::string archive_name = GetArchiveBaseName(archive_path);
        std::string base_dest = extract_dest;
        if (base_dest.back() != '/')
            base_dest += '/';
        base_dest += archive_name + '/';
        
        // Create the base extraction directory
        std::error_code ec;
        std::filesystem::create_directories(base_dest, ec);
        if (ec) {
            Log::Error("Archive::Extract - Failed to create extraction directory: %s\n", base_dest.c_str());
            archive_read_free(a);
            return false;
        }
        
        const std::size_t buf_size = 0x10000;  // 64KB buffer
        auto buffer = std::make_unique<unsigned char[]>(buf_size);
        
        int64_t entries_extracted = 0;
        struct archive_entry *entry;
        
        while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
            const char *entry_path = archive_entry_pathname(entry);
            if (!entry_path) {
                archive_read_data_skip(a);
                continue;
            }
            
            std::string dest_path = base_dest + entry_path;
            mode_t entry_type = archive_entry_filetype(entry);
            
            if (entry_type == AE_IFDIR) {
                // Create directory
                std::filesystem::create_directories(dest_path, ec);
            }
            else if (entry_type == AE_IFREG) {
                // Extract regular file
                // First create parent directories
                std::filesystem::path p(dest_path);
                std::filesystem::create_directories(p.parent_path(), ec);
                
                FILE *out_file = fopen(dest_path.c_str(), "wb");
                if (!out_file) {
                    Log::Error("Archive::Extract - Failed to create output file: %s\n", dest_path.c_str());
                    archive_read_data_skip(a);
                    continue;
                }
                
                la_ssize_t bytes_read;
                while ((bytes_read = archive_read_data(a, buffer.get(), buf_size)) > 0) {
                    fwrite(buffer.get(), 1, bytes_read, out_file);
                }
                
                if (bytes_read < 0) {
                    Log::Error("Archive::Extract - Error reading data: %s (%s)\n", 
                              entry_path, archive_error_string(a));
                }
                
                fclose(out_file);
            }
            else if (entry_type == AE_IFLNK) {
                // Symbolic link - skip on Switch (no symlink support)
                Log::Debug("Archive::Extract - Skipping symlink: %s\n", entry_path);
                archive_read_data_skip(a);
            }
            else {
                // Other types (sockets, devices, etc.) - skip
                archive_read_data_skip(a);
            }
            
            entries_extracted++;
            
            // Update progress
            std::string short_filename = entry_path;
            if (short_filename.length() > 40) {
                short_filename = "..." + short_filename.substr(short_filename.length() - 37);
            }
            std::string progress_text = extracting_prefix + " " + short_filename;
            
            if (total_entries > 0) {
                Popups::ProgressBar(app, static_cast<float>(entries_extracted), 
                                   static_cast<float>(total_entries), title, progress_text);
            } else {
                // Indeterminate progress (show count only)
                Popups::ProgressBar(app, 0.0f, 0.0f, title, progress_text);
            }
        }
        
        int archive_result = archive_read_close(a);
        archive_read_free(a);
        
        if (archive_result != ARCHIVE_OK) {
            Log::Error("Archive::Extract - Archive closed with errors\n");
        }
        
        Log::Debug("Archive::Extract - Extracted %lld entries to %s\n", entries_extracted, base_dest.c_str());
        return true;
    }
}

namespace Popups {
    void ArchivePopup(App &app) {
        const int lang = app.config.Lang();
        Popups::SetupPopup(app, strings[lang][Lang::ArchiveTitle]);
        
        if (ImGui::BeginPopupModal(strings[lang][Lang::ArchiveTitle], nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar)) {
            ImGui::Text("%s", strings[lang][Lang::ArchiveMessage]);
            
            // Get just the filename from the path
            std::filesystem::path p(Archive::GetArchivePath());
            std::string filename = p.filename().string();
            
            std::string prompt = strings[lang][Lang::ArchivePrompt] + filename + "?";
            ImGui::Text("%s", prompt.c_str());
            
            ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
            
            if (ImGui::Button(strings[lang][Lang::ButtonOK], ImVec2(120, 36))) {
                ImGui::EndPopup();
                ImGui::PopStyleVar();
                ImGui::Render();
                
                bool success = Archive::Extract(app);
                if (!success) {
                    Log::Error("Archive extraction failed\n");
                    Toast::Show(strings[lang][Lang::ArchiveError], false, 3.0f);
                } else {
                    Toast::Show(strings[lang][Lang::ArchiveSuccess], true, 3.0f);
                }
                
                // Refresh directory listing
                if (!FS::RefreshDirectory(app.fs, app.selection, app.window.entries, app.window.metadata_cache, true)) {
                    Log::Error("ArchivePopup: Failed to refresh directory after extraction\n");
                }
                app.window.sort = -1;
                
                app.window.state = WINDOW_STATE_FILEBROWSER;
                return;
            }
            ImGui::SetItemDefaultFocus();
            
            ImGui::SameLine(0.0f, 15.0f);
            
            if (ImGui::Button(strings[lang][Lang::ButtonCancel], ImVec2(120, 36))) {
                ImGui::CloseCurrentPopup();
                app.window.state = WINDOW_STATE_FILEBROWSER;
            }
        }
        
        Popups::ExitPopup();
    }
}
