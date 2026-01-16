#include <cstdio>
#include <cstring>
#include <jansson.h>

#include "config.hpp"
#include "fs.hpp"
#include "log.hpp"

#define CONFIG_VERSION 6

config_t cfg;

namespace Config {
    static const char *config_path = "/switch/NX-Shell/config.json";
    static const char *config_file = "{\n\t\"config_version\": %d,\n\t\"language\": %d,\n\t\"dev_options\": %d,\n\t\"image_filename\": %d,\n\t\"multi_lang\": %d,\n\t\"resolution_mode\": %d\n}";
    static int config_version_holder = 0;
    static const int buf_size = 128;
    
    int Save(config_t &config) {
        Result ret = 0;
        char *buf = new char[buf_size];
        u64 len = std::snprintf(buf, buf_size, config_file, CONFIG_VERSION, config.lang, config.dev_options, config.image_filename, config.multi_lang, config.resolution_mode);
        
        // Delete and re-create the file, we don't care about the return value here.
        fsFsDeleteFile(std::addressof(devices[FileSystemSDMC]), config_path);
        fsFsCreateFile(std::addressof(devices[FileSystemSDMC]), config_path, len, 0);
        
        FsFile file;
        if (R_FAILED(ret = fsFsOpenFile(std::addressof(devices[FileSystemSDMC]), config_path, FsOpenMode_Write, std::addressof(file)))) {
            Log::Error("Config::Save fsFsOpenFile(%s) failed: 0x%x\n", config_path, ret);
            delete[] buf;
            return ret;
        }
        
        if (R_FAILED(ret = fsFileWrite(std::addressof(file), 0, buf, len, FsWriteOption_Flush))) {
            Log::Error("Config::Save fsFileWrite(%s) failed: 0x%x\n", config_path, ret);
            delete[] buf;
            fsFileClose(std::addressof(file));
            return ret;
        }
        
        fsFileClose(std::addressof(file));
        delete[] buf;
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

        json_t *multi_lang = json_object_get(root, "multi_lang");
        cfg.multi_lang = json_integer_value(multi_lang);

        json_t *resolution_mode = json_object_get(root, "resolution_mode");
        cfg.resolution_mode = json_integer_value(resolution_mode);

        json_decref(root);
        return 0;
    }

    // Check if a language index is supported (has proper translations)
    static bool IsLanguageSupported(int lang_index) {
        // Supported languages: English(1), German(3), Spanish(5), 
        // Simplified Chinese(6), Korean(7), Portuguese(9), Traditional Chinese(11)
        switch (lang_index) {
            case 1:  // English
            case 3:  // German
            case 5:  // Spanish
            case 6:  // Simplified Chinese
            case 7:  // Korean
            case 9:  // Portuguese
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
