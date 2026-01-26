#include <cstring>
#include <filesystem>
#include <minizip/unzip.h>

#include "archive.hpp"
#include "config.hpp"
#include "fs.hpp"
#include "imgui.h"
#include "language.hpp"
#include "log.hpp"
#include "popups.hpp"
#include "selection.hpp"
#include "windows.hpp"

namespace Archive {
    static std::string archive_path;
    static std::string extract_dest;
    static bool extraction_in_progress = false;
    static bool extraction_complete = false;
    static bool extraction_error = false;
    static std::string current_file;
    static float progress = 0.0f;
    
    void SetArchivePath(const std::string &path) {
        archive_path = path;
        // Default extraction destination: same directory as the archive
        std::filesystem::path p(path);
        extract_dest = p.parent_path().string();
        if (extract_dest.empty()) {
            extract_dest = device + cwd;
        }
        extraction_in_progress = false;
        extraction_complete = false;
        extraction_error = false;
        current_file.clear();
        progress = 0.0f;
    }
    
    const std::string& GetArchivePath(void) {
        return archive_path;
    }
    
    static bool CreateDirectories(const std::string &path) {
        std::filesystem::path p(path);
        std::error_code ec;
        std::filesystem::create_directories(p.parent_path(), ec);
        return !ec;
    }
    
    bool ExtractZip(void) {
        if (archive_path.empty()) {
            Log::Error("Archive::ExtractZip - No archive path set\n");
            return false;
        }
        
        unzFile zip = unzOpen64(archive_path.c_str());
        if (!zip) {
            Log::Error("Archive::ExtractZip - Failed to open archive: %s\n", archive_path.c_str());
            return false;
        }
        
        // Get global info to determine total files
        unz_global_info64 global_info;
        if (unzGetGlobalInfo64(zip, &global_info) != UNZ_OK) {
            Log::Error("Archive::ExtractZip - Failed to get global info\n");
            unzClose(zip);
            return false;
        }
        
        const int lang = Config::GetLang();
        const std::string title = strings[lang][Lang::ArchiveTitle];
        const std::string extracting_prefix = strings[lang][Lang::ArchiveExtracting];
        
        // Create base extraction directory (archive name without extension)
        std::filesystem::path archive_p(archive_path);
        std::string archive_name = archive_p.stem().string();
        std::string base_dest = extract_dest;
        if (base_dest.back() != '/')
            base_dest += '/';
        base_dest += archive_name + '/';
        
        // Create the base extraction directory
        std::error_code ec;
        std::filesystem::create_directories(base_dest, ec);
        if (ec) {
            Log::Error("Archive::ExtractZip - Failed to create extraction directory: %s\n", base_dest.c_str());
            unzClose(zip);
            return false;
        }
        
        const std::size_t buf_size = 0x10000;  // 64KB buffer
        unsigned char *buffer = new unsigned char[buf_size];
        
        u64 files_extracted = 0;
        int ret = unzGoToFirstFile(zip);
        
        while (ret == UNZ_OK) {
            unz_file_info64 file_info;
            char filename[512];
            
            if (unzGetCurrentFileInfo64(zip, &file_info, filename, sizeof(filename), nullptr, 0, nullptr, 0) != UNZ_OK) {
                Log::Error("Archive::ExtractZip - Failed to get file info\n");
                ret = unzGoToNextFile(zip);
                continue;
            }
            
            std::string dest_path = base_dest + filename;
            
            // Check if it's a directory (ends with /)
            size_t filename_len = std::strlen(filename);
            if (filename_len > 0 && filename[filename_len - 1] == '/') {
                // Create directory
                std::filesystem::create_directories(dest_path, ec);
            }
            else {
                // Extract file
                // First create parent directories
                CreateDirectories(dest_path);
                
                if (unzOpenCurrentFile(zip) != UNZ_OK) {
                    Log::Error("Archive::ExtractZip - Failed to open file in archive: %s\n", filename);
                    ret = unzGoToNextFile(zip);
                    continue;
                }
                
                FILE *out_file = fopen(dest_path.c_str(), "wb");
                if (!out_file) {
                    Log::Error("Archive::ExtractZip - Failed to create output file: %s\n", dest_path.c_str());
                    unzCloseCurrentFile(zip);
                    ret = unzGoToNextFile(zip);
                    continue;
                }
                
                int bytes_read;
                do {
                    bytes_read = unzReadCurrentFile(zip, buffer, buf_size);
                    if (bytes_read < 0) {
                        Log::Error("Archive::ExtractZip - Error reading file: %s\n", filename);
                        break;
                    }
                    if (bytes_read > 0) {
                        fwrite(buffer, 1, bytes_read, out_file);
                    }
                } while (bytes_read > 0);
                
                fclose(out_file);
                unzCloseCurrentFile(zip);
            }
            
            files_extracted++;
            
            // Update progress
            std::string short_filename = filename;
            if (short_filename.length() > 40) {
                short_filename = "..." + short_filename.substr(short_filename.length() - 37);
            }
            std::string progress_text = extracting_prefix + " " + short_filename;
            Popups::ProgressBar(static_cast<float>(files_extracted), static_cast<float>(global_info.number_entry), title, progress_text);
            
            ret = unzGoToNextFile(zip);
        }
        
        delete[] buffer;
        unzClose(zip);
        
        Log::Debug("Archive::ExtractZip - Extracted %lu files to %s\n", files_extracted, base_dest.c_str());
        return true;
    }
}

namespace Popups {
    void ArchivePopup(void) {
        const int lang = Config::GetLang();
        Popups::SetupPopup(strings[lang][Lang::ArchiveTitle]);
        
        if (ImGui::BeginPopupModal(strings[lang][Lang::ArchiveTitle], nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("%s", strings[lang][Lang::ArchiveMessage]);
            
            // Get just the filename from the path
            std::filesystem::path p(Archive::GetArchivePath());
            std::string filename = p.filename().string();
            
            std::string prompt = strings[lang][Lang::ArchivePrompt] + filename + "?";
            ImGui::Text("%s", prompt.c_str());
            
            ImGui::Dummy(ImVec2(0.0f, 5.0f)); // Spacing
            
            if (ImGui::Button(strings[lang][Lang::ButtonOK], ImVec2(120, 0))) {
                ImGui::EndPopup();
                ImGui::PopStyleVar();
                ImGui::Render();
                
                if (!Archive::ExtractZip()) {
                    Log::Error("Archive extraction failed\n");
                }
                
                // Refresh directory listing
                if (!FS::RefreshDirectory(data.entries, data.metadata_cache, true)) {
                    Log::Error("ArchivePopup: Failed to refresh directory after extraction\n");
                }
                sort = -1;
                
                data.state = WINDOW_STATE_FILEBROWSER;
                return;
            }
            
            ImGui::SameLine(0.0f, 15.0f);
            
            if (ImGui::Button(strings[lang][Lang::ButtonCancel], ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
                data.state = WINDOW_STATE_FILEBROWSER;
            }
        }
        
        Popups::ExitPopup();
    }
}
