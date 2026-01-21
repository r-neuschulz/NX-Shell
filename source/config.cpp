#include <cstdio>
#include <cstring>
#include <jansson.h>

#include "config.hpp"
#include "fs.hpp"
#include "log.hpp"

#define CONFIG_VERSION 16

config_t cfg;

namespace Config {
    static const char *config_path = "/switch/NX-Shell/config.json";
    static int config_version_holder = 0;
    
    // JSON helper functions to reduce repetition
    static inline void SetInt(json_t *obj, const char *key, int val) {
        json_object_set_new(obj, key, json_integer(val));
    }
    
    static inline void SetString(json_t *obj, const char *key, const std::string &val) {
        json_object_set_new(obj, key, json_string(val.c_str()));
    }
    
    static inline int GetInt(json_t *root, const char *key) {
        return static_cast<int>(json_integer_value(json_object_get(root, key)));
    }
    
    static inline std::string GetString(json_t *root, const char *key, const std::string &fallback = "") {
        json_t *val = json_object_get(root, key);
        return (val && json_is_string(val)) ? json_string_value(val) : fallback;
    }
    
    static inline void EnsureDirExists(const char *path) {
        if (!FS::DirExists(path))
            fsFsCreateDirectory(std::addressof(devices[FileSystemSDMC]), path);
    }
    
    int Save(config_t &config) {
        Result ret = 0;
        
        // Build JSON using jansson for proper string escaping
        json_t *root = json_object();
        SetInt(root, "config_version", CONFIG_VERSION);
        SetInt(root, "language", config.lang);
        SetInt(root, "dev_options", config.dev_options);
        SetInt(root, "image_filename", config.image_filename);
        SetInt(root, "enter_images_fullscreen", config.enter_images_fullscreen);
        SetInt(root, "resolution_mode", config.resolution_mode);
        SetInt(root, "theme_mode", config.theme_mode);
        SetInt(root, "show_details", config.show_details);
        SetInt(root, "show_stats", config.show_stats);
        SetString(root, "last_device", config.last_device);
        SetString(root, "last_cwd", config.last_cwd);
        
        // Save accent color as array of 3 floats
        json_t *accent_array = json_array();
        for (int i = 0; i < 3; i++)
            json_array_append_new(accent_array, json_real(config.accent_color[i]));
        json_object_set_new(root, "accent_color", accent_array);
        
        SetInt(root, "button_style", config.button_style);
        
        char *buf = json_dumps(root, JSON_INDENT(1));
        json_decref(root);
        
        if (!buf) {
            Log::Error("Config::Save json_dumps failed\n");
            return -1;
        }
        
        u64 len = std::strlen(buf);
        
        Log::Debug("Config::Save - lang=%d, dev_options=%d, show_stats=%d, last_device=%s, last_cwd=%s, len=%llu\n", 
                   config.lang, config.dev_options, config.show_stats, config.last_device.c_str(), config.last_cwd.c_str(), len);
        
        // Delete and re-create the file, we don't care about the return value here.
        fsFsDeleteFile(std::addressof(devices[FileSystemSDMC]), config_path);
        fsFsCreateFile(std::addressof(devices[FileSystemSDMC]), config_path, len, 0);
        
        FsFile file;
        if (R_FAILED(ret = fsFsOpenFile(std::addressof(devices[FileSystemSDMC]), config_path, FsOpenMode_Write, std::addressof(file)))) {
            Log::Error("Config::Save fsFsOpenFile(%s) failed: 0x%x\n", config_path, ret);
            free(buf);
            return ret;
        }
        
        if (R_FAILED(ret = fsFileWrite(std::addressof(file), 0, buf, len, FsWriteOption_Flush))) {
            Log::Error("Config::Save fsFileWrite(%s) failed: 0x%x\n", config_path, ret);
            free(buf);
            fsFileClose(std::addressof(file));
            return ret;
        }
        
        fsFileClose(std::addressof(file));
        free(buf);
        return 0;
    }
    
    int Load(void) {
        Result ret = 0;
        
        EnsureDirExists("/switch/");
        EnsureDirExists("/switch/NX-Shell/");
            
        if (!FS::FileExists(config_path)) {
            cfg = {};
            return Config::Save(cfg);
        }
        
        FsFile file;
        if (R_FAILED(ret = fsFsOpenFile(std::addressof(devices[FileSystemSDMC]), config_path, FsOpenMode_Read, std::addressof(file))))
            return ret;
        
        s64 size = 0;
        if (R_FAILED(ret = fsFileGetSize(std::addressof(file), std::addressof(size)))) {
            fsFileClose(std::addressof(file));
            return ret;
        }

        char *buf = new char[size + 1];
        if (R_FAILED(ret = fsFileRead(std::addressof(file), 0, buf, static_cast<u64>(size) + 1, FsReadOption_None, nullptr))) {
            delete[] buf;
            fsFileClose(std::addressof(file));
            return ret;
        }
        
        fsFileClose(std::addressof(file));
        
        json_t *root;
        json_error_t error;
        root = json_loads(buf, 0, std::addressof(error));
        delete[] buf;
        
        if (!root) {
            std::printf("error: on line %d: %s\n", error.line, error.text);
            return -1;
        }
        
        config_version_holder = GetInt(root, "config_version");

        // Delete config file if config file is updated. This will rarely happen.
        if (config_version_holder < CONFIG_VERSION) {
            json_decref(root);
            fsFsDeleteFile(std::addressof(devices[FileSystemSDMC]), config_path);
            cfg = {};
            return Config::Save(cfg);
        }

        // Load integer/boolean fields
        cfg.lang = GetInt(root, "language");
        cfg.dev_options = GetInt(root, "dev_options");
        cfg.image_filename = GetInt(root, "image_filename");
        cfg.enter_images_fullscreen = GetInt(root, "enter_images_fullscreen");
        cfg.resolution_mode = GetInt(root, "resolution_mode");
        cfg.theme_mode = GetInt(root, "theme_mode");
        cfg.show_details = GetInt(root, "show_details");
        cfg.show_stats = GetInt(root, "show_stats");
        cfg.button_style = GetInt(root, "button_style");

        // Load string fields with defaults
        cfg.last_device = GetString(root, "last_device", "sdmc:");
        cfg.last_cwd = GetString(root, "last_cwd", "/");

        // Load accent color array
        json_t *accent_color = json_object_get(root, "accent_color");
        if (accent_color && json_is_array(accent_color) && json_array_size(accent_color) == 3) {
            for (int i = 0; i < 3; i++)
                cfg.accent_color[i] = static_cast<float>(json_real_value(json_array_get(accent_color, i)));
        }

        json_decref(root);
        return 0;
    }

    // Check if a language index is supported (has proper translations)
    static bool IsLanguageSupported(int lang_index) {
        // All languages 0-11 are supported
        switch (lang_index) {
            case 0:  // Japanese
            case 1:  // English
            case 2:  // French
            case 3:  // German
            case 4:  // Italian
            case 5:  // Spanish
            case 6:  // Simplified Chinese
            case 7:  // Korean
            case 8:  // Dutch
            case 9:  // Portuguese
            case 10: // Russian
            case 11: // Traditional Chinese
                return true;
            default:
                return false;
        }
    }

    int GetLang(void) {
        if (cfg.lang == LANG_AUTO) {
            u64 lang_code = 0;
            SetLanguage lang = SetLanguage_ENUS;
            
            if (R_SUCCEEDED(setGetSystemLanguage(&lang_code))) {
                if (R_SUCCEEDED(setMakeLanguage(lang_code, &lang))) {
                    int lang_index = static_cast<int>(lang);
                    // Only return if it's a supported language
                    if (IsLanguageSupported(lang_index)) {
                        return lang_index;
                    }
                }
            }
            // Default to English if detection fails or language unsupported
            return 1; // English
        }
        
        // For manual selection, verify it's supported (shouldn't happen with UI, but safety check)
        if (IsLanguageSupported(cfg.lang)) {
            return cfg.lang;
        }
        return 1; // English fallback
    }
}
