#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <curl/curl.h>
#include <jansson.h>
#include <switch.h>

#include "config.hpp"
#include "fs.hpp"
#include "log.hpp"
#include "net.hpp"
#include "services.hpp"
#include "utils.hpp"

// Custom deleters for RAII resource management
namespace {
    struct CurlDeleter {
        void operator()(CURL *handle) const { if (handle) curl_easy_cleanup(handle); }
    };
    struct CurlSlistDeleter {
        void operator()(curl_slist *list) const { if (list) curl_slist_free_all(list); }
    };
    struct JsonDeleter {
        void operator()(json_t *json) const { if (json) json_decref(json); }
    };
    
    using CurlHandle = std::unique_ptr<CURL, CurlDeleter>;
    using CurlSlist = std::unique_ptr<curl_slist, CurlSlistDeleter>;
    using JsonPtr = std::unique_ptr<json_t, JsonDeleter>;
}

namespace Net {
    static s64 offset = 0;
    
    // Socket initialization state (lazy init for faster startup)
    static bool s_socket_initialized = false;
    
    // Initialize socket with nxlink stdio (for dev_options mode)
    void InitSocketWithNxlink(void) {
        if (s_socket_initialized)
            return;
        
        socketInitializeDefault();
        nxlinkStdio();
        s_socket_initialized = true;
    }
    
    // Ensure socket is ready before network operations (lazy init if needed)
    void EnsureSocketReady(void) {
        if (s_socket_initialized)
            return;
        
        // Lazy init without nxlink (for normal users checking updates)
        socketInitializeDefault();
        s_socket_initialized = true;
    }
    
    // Check if socket was initialized (for cleanup)
    bool IsSocketInitialized(void) {
        return s_socket_initialized;
    }
    
    // Cleanup socket on exit
    void ExitSocket(void) {
        if (s_socket_initialized) {
            socketExit();
            s_socket_initialized = false;
        }
    }

    bool GetNetworkStatus(void) {
        Result ret = 0;
        NifmInternetConnectionStatus status;
        if (R_FAILED(ret = nifmGetInternetConnectionStatus(nullptr, nullptr, std::addressof(status)))) {
            Log::Error("nifmGetInternetConnectionStatus() failed: 0x%x\n", ret);
            return false;
        }

        return (status == NifmInternetConnectionStatus_Connected);
    }

    bool GetAvailableUpdate(const std::string &tag) {
        if (tag.empty())
            return false;
            
        int current_ver = ((VERSION_MAJOR * 100) + (VERSION_MINOR * 10) + VERSION_MICRO);

        std::string tag_name = tag;
        // Strip 'v' prefix if present (e.g., "v5.0.1" -> "5.0.1")
        if (!tag_name.empty() && (tag_name[0] == 'v' || tag_name[0] == 'V'))
            tag_name.erase(0, 1);
        // Remove dots (e.g., "5.0.1" -> "501")
        tag_name.erase(std::remove_if(tag_name.begin(), tag_name.end(), [](char c) { return c == '.'; }), tag_name.end());
        
        // Safely parse version number (no exceptions - project uses -fno-exceptions)
        if (tag_name.empty())
            return false;
        
        // Use strtol for safe parsing without exceptions
        char *end = nullptr;
        long available_ver = std::strtol(tag_name.c_str(), &end, 10);
        if (end == tag_name.c_str() || *end != '\0') {
            Log::Error("Failed to parse version tag: %s\n", tag.c_str());
            return false;
        }
        
        return (available_ver > current_ver);
    }
    
    size_t WriteJSONData(const char *ptr, size_t size, size_t nmemb, void *userdata) {
        const size_t total_size(size * nmemb);
        reinterpret_cast<std::string *>(userdata)->append(ptr, total_size);
        return total_size;
    }
    
    // Stringify macros for compile-time version string
    #define NET_STRINGIFY(x) #x
    #define NET_TOSTRING(x) NET_STRINGIFY(x)
    #define NX_SHELL_USER_AGENT "NX-Shell/" NET_TOSTRING(VERSION_MAJOR) "." NET_TOSTRING(VERSION_MINOR) "." NET_TOSTRING(VERSION_MICRO)
    
    // Configure common CURL options for GitHub API requests
    static void SetupCurlCommon(CURL *handle, const char *url) {
        curl_easy_setopt(handle, CURLOPT_URL, url);
        curl_easy_setopt(handle, CURLOPT_USERAGENT, NX_SHELL_USER_AGENT);
        curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
    }

    std::string GetLatestReleaseJSON(void) {
        // Ensure socket is ready before network operations (lazy init if needed)
        EnsureSocketReady();
        
        std::string json;
        CurlHandle handle(curl_easy_init());
        if (!handle) {
            Log::Error("curl_easy_init() failed\n");
            return std::string();
        }
        
        CurlSlist header_data(curl_slist_append(nullptr, "Content-Type: application/json"));
        header_data.reset(curl_slist_append(header_data.release(), "Accept: application/json"));
        curl_easy_setopt(handle.get(), CURLOPT_HTTPHEADER, header_data.get());
        
        // Use compile-time GitHub repository info (injected via CMake)
        std::string api_url = "https://api.github.com/repos/" GITHUB_REPO_OWNER "/" GITHUB_REPO_NAME "/releases/latest";
        Log::Debug("Checking for updates at: %s\n", api_url.c_str());
        SetupCurlCommon(handle.get(), api_url.c_str());
        curl_easy_setopt(handle.get(), CURLOPT_WRITEFUNCTION, Net::WriteJSONData);
        curl_easy_setopt(handle.get(), CURLOPT_WRITEDATA, std::addressof(json));
        curl_easy_perform(handle.get());
        
        json_error_t error;
        JsonPtr root(json_loads(json.c_str(), JSON_DECODE_ANY, std::addressof(error)));
        
        if (!root) {
            Log::Error("json_loads failed on line %d: %s\n", error.line, error.text);
            return std::string();
        }
        
        json_t *tag = json_object_get(root.get(), "tag_name");
        if (!tag || !json_is_string(tag)) {
            Log::Error("tag_name not found in release JSON\n");
            return std::string();
        }
        
        const char *tag_value = json_string_value(tag);
        return tag_value ? std::string(tag_value) : std::string();
    }

    size_t WriteNROData(const char *ptr, size_t size, size_t nmemb, void *userdata) {
        if (R_SUCCEEDED(fsFileWrite(reinterpret_cast<FsFile *>(userdata), offset, ptr, (size * nmemb), FsWriteOption_Flush)))
            offset += (size * nmemb);
            
        return (size * nmemb);
    }
    
    std::string GetLatestReleaseNRO(FileSystemService &fs_svc, const std::string &tag) {
        // #region agent log - Hypothesis A,D,E: Log entry point and parameters
        Log::Debug("[DBGMODE] GetLatestReleaseNRO: tag='%s', app_path='%s'\n", tag.c_str(), __application_path);
        // #endregion
        
        // Ensure socket is ready before network operations (lazy init if needed)
        EnsureSocketReady();
        
        // Build versioned filename from tag (e.g., "v5.0.1" -> "NX-Shell-v5-0-1.nro")
        std::string version_dashed = tag;
        for (char &c : version_dashed) {
            if (c == '.') c = '-';
        }
        std::string nro_filename = "NX-Shell-" + version_dashed + ".nro";
        
        // Build download path next to the running NRO with the versioned filename
        // e.g., "sdmc:/switch/NX-Shell-v5-0-0.nro" -> "sdmc:/switch/NX-Shell-v5-0-1.nro"
        std::string app_path(__application_path);
        std::string download_path;
        
        // Find the last '/' to get the directory
        size_t last_slash = app_path.rfind('/');
        if (last_slash != std::string::npos) {
            download_path = app_path.substr(0, last_slash + 1) + nro_filename;
        } else {
            // Fallback if no slash found (shouldn't happen)
            download_path = "/switch/" + nro_filename;
        }
        
        // Remove "sdmc:" prefix for filesystem operations
        std::string fs_path = download_path;
        if (fs_path.rfind("sdmc:", 0) == 0) {
            fs_path = fs_path.substr(5);
        }
        
        // #region agent log - Hypothesis D: Log computed paths
        Log::Debug("[DBGMODE] Computed paths: nro_filename='%s', download_path='%s', fs_path='%s'\n", 
                   nro_filename.c_str(), download_path.c_str(), fs_path.c_str());
        // #endregion
        
        Result ret = 0;
        FsFile file;

        if (!FS::FileExists(fs_path))
            fsFsCreateFile(std::addressof(fs_svc.devices[FileSystemSDMC]), fs_path.c_str(), 0, 0);

        if (R_FAILED(ret = fsFsOpenFile(std::addressof(fs_svc.devices[FileSystemSDMC]), fs_path.c_str(), FsOpenMode_Write | FsOpenMode_Append, std::addressof(file)))) {
            Log::Error("fsFsOpenFile(%s) failed: 0x%x\n", fs_path.c_str(), ret);
            return "";
        }
        
        bool download_success = false;
        CurlHandle handle(curl_easy_init());
        if (handle) {
            // Use compile-time GitHub repository info (injected via CMake)
            std::string url = "https://github.com/" GITHUB_REPO_OWNER "/" GITHUB_REPO_NAME "/releases/download/" + tag + "/" + nro_filename;
            Log::Debug("Downloading update from: %s\n", url.c_str());
            Log::Debug("Saving to: %s\n", fs_path.c_str());
            SetupCurlCommon(handle.get(), url.c_str());
            curl_easy_setopt(handle.get(), CURLOPT_WRITEFUNCTION, Net::WriteNROData);
            curl_easy_setopt(handle.get(), CURLOPT_WRITEDATA, std::addressof(file));
            curl_easy_setopt(handle.get(), CURLOPT_NOPROGRESS, 0L);
            
            // #region agent log - Hypothesis A: Check curl result
            CURLcode curl_result = curl_easy_perform(handle.get());
            long http_code = 0;
            curl_easy_getinfo(handle.get(), CURLINFO_RESPONSE_CODE, &http_code);
            Log::Debug("[DBGMODE] curl_easy_perform result: %d (%s), HTTP code: %ld, bytes_written: %lld\n", 
                       curl_result, curl_easy_strerror(curl_result), http_code, (long long)offset);
            // #endregion
            
            // FIX: Check HTTP response code - only consider success if 200 OK
            download_success = (curl_result == CURLE_OK && http_code == 200);
            if (!download_success) {
                Log::Error("Download failed: curl_result=%d, http_code=%ld\n", curl_result, http_code);
            }
        }
        
        // #region agent log - Hypothesis B,C: Check file size before close
        s64 file_size = 0;
        fsFileGetSize(&file, &file_size);
        Log::Debug("[DBGMODE] File size before close: %lld bytes, offset: %lld\n", (long long)file_size, (long long)offset);
        // #endregion
        
        fsFileClose(std::addressof(file));
        offset = 0;
        
        // FIX: If download failed, delete the corrupted/partial file and return empty
        if (!download_success) {
            Log::Debug("[DBGMODE] Download failed, deleting corrupted file: %s\n", fs_path.c_str());
            fsFsDeleteFile(std::addressof(fs_svc.devices[FileSystemSDMC]), fs_path.c_str());
            return "";
        }
        
        // #region agent log - Hypothesis B: Verify file exists and size after close
        s64 post_close_size = 0;
        FsFile verify_file;
        if (R_SUCCEEDED(fsFsOpenFile(std::addressof(fs_svc.devices[FileSystemSDMC]), fs_path.c_str(), FsOpenMode_Read, std::addressof(verify_file)))) {
            fsFileGetSize(&verify_file, &post_close_size);
            fsFileClose(&verify_file);
        }
        Log::Debug("[DBGMODE] File size after close (verified): %lld bytes, file_exists: %d\n", 
                   (long long)post_close_size, FS::FileExists(fs_path));
        // #endregion
        
        // #region agent log - Hypothesis D: Log return value
        Log::Debug("[DBGMODE] GetLatestReleaseNRO returning: '%s'\n", fs_path.c_str());
        // #endregion
        
        return fs_path;
    }
}
