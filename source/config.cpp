#include <cstdio>
#include <cstring>
#include <jansson.h>

#include "config.hpp"
#include "fs.hpp"
#include "log.hpp"

#define CONFIG_VERSION 15

config_t cfg;

namespace Config {
    static const char *config_path = "/switch/NX-Shell/config.json";
    static int config_version_holder = 0;
    
    int Save(config_t &config) {
        Result ret = 0;
        
        // Build JSON using jansson for proper string escaping
        json_t *root = json_object();
        json_object_set_new(root, "config_version", json_integer(CONFIG_VERSION));
        json_object_set_new(root, "language", json_integer(config.lang));
        json_object_set_new(root, "dev_options", json_integer(config.dev_options));
        json_object_set_new(root, "image_filename", json_integer(config.image_filename));
        json_object_set_new(root, "enter_images_fullscreen", json_integer(config.enter_images_fullscreen));
        json_object_set_new(root, "multi_lang", json_integer(config.multi_lang));
        json_object_set_new(root, "resolution_mode", json_integer(config.resolution_mode));
        json_object_set_new(root, "theme_mode", json_integer(config.theme_mode));
        json_object_set_new(root, "show_details", json_integer(config.show_details));
        json_object_set_new(root, "show_stats", json_integer(config.show_stats));
        json_object_set_new(root, "last_device", json_string(config.last_device.c_str()));
        json_object_set_new(root, "last_cwd", json_string(config.last_cwd.c_str()));
        
        // Save accent color as array of 3 floats
        json_t *accent_array = json_array();
        json_array_append_new(accent_array, json_real(config.accent_color[0]));
        json_array_append_new(accent_array, json_real(config.accent_color[1]));
        json_array_append_new(accent_array, json_real(config.accent_color[2]));
        json_object_set_new(root, "accent_color", accent_array);
        
        json_object_set_new(root, "button_style", json_integer(config.button_style));
        json_object_set_new(root, "nxmp_nro_path", json_string(config.nxmp_nro_path.c_str()));
        
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
        
        if (!FS::DirExists("/switch/"))
            fsFsCreateDirectory(std::addressof(devices[FileSystemSDMC]), "/switch");
        if (!FS::DirExists("/switch/NX-Shell/"))
            fsFsCreateDirectory(std::addressof(devices[FileSystemSDMC]), "/switch/NX-Shell");
            
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

        char *buf =  new char[size + 1];
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
        
        json_t *config_ver = json_object_get(root, "config_version");
        config_version_holder = json_integer_value(config_ver);

        // Delete config file if config file is updated. This will rarely happen.
        if (config_version_holder < CONFIG_VERSION) {
            fsFsDeleteFile(std::addressof(devices[FileSystemSDMC]), config_path);
            cfg = {};
            return Config::Save(cfg);
        }

        json_t *language = json_object_get(root, "language");
        cfg.lang = json_integer_value(language);
        
        json_t *dev_options = json_object_get(root, "dev_options");
        cfg.dev_options = json_integer_value(dev_options);

        json_t *image_filename = json_object_get(root, "image_filename");
        cfg.image_filename = json_integer_value(image_filename);

        json_t *enter_images_fullscreen = json_object_get(root, "enter_images_fullscreen");
        cfg.enter_images_fullscreen = json_integer_value(enter_images_fullscreen);

        json_t *multi_lang = json_object_get(root, "multi_lang");
        cfg.multi_lang = json_integer_value(multi_lang);

        json_t *resolution_mode = json_object_get(root, "resolution_mode");
        cfg.resolution_mode = json_integer_value(resolution_mode);

        json_t *theme_mode = json_object_get(root, "theme_mode");
        cfg.theme_mode = json_integer_value(theme_mode);

        json_t *show_details = json_object_get(root, "show_details");
        cfg.show_details = json_integer_value(show_details);

        json_t *show_stats = json_object_get(root, "show_stats");
        cfg.show_stats = json_integer_value(show_stats);

        json_t *last_device = json_object_get(root, "last_device");
        if (last_device && json_is_string(last_device))
            cfg.last_device = json_string_value(last_device);
        
        json_t *last_cwd = json_object_get(root, "last_cwd");
        if (last_cwd && json_is_string(last_cwd))
            cfg.last_cwd = json_string_value(last_cwd);

        // Load accent color
        json_t *accent_color = json_object_get(root, "accent_color");
        if (accent_color && json_is_array(accent_color) && json_array_size(accent_color) == 3) {
            cfg.accent_color[0] = static_cast<float>(json_real_value(json_array_get(accent_color, 0)));
            cfg.accent_color[1] = static_cast<float>(json_real_value(json_array_get(accent_color, 1)));
            cfg.accent_color[2] = static_cast<float>(json_real_value(json_array_get(accent_color, 2)));
        }

        json_t *button_style = json_object_get(root, "button_style");
        cfg.button_style = json_integer_value(button_style);

        json_t *nxmp_nro_path = json_object_get(root, "nxmp_nro_path");
        if (nxmp_nro_path && json_is_string(nxmp_nro_path))
            cfg.nxmp_nro_path = json_string_value(nxmp_nro_path);

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
