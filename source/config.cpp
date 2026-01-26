#include <cstdio>
#include <cstring>
#include <memory>
#include <sys/stat.h>
#include <jansson.h>

#include "config.hpp"
#include "log.hpp"
#include "services.hpp"

#define CONFIG_VERSION 17

namespace Config {
    static const char *config_path = "/switch/NX-Shell/config.json";
    static int config_version_holder = 0;
    
    // JSON helpers
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
    
    static inline void EnsureDirExists(FsFileSystem *sdmc_fs, const char *path) {
        struct stat dir_stat = { 0 };
        if (stat(path, &dir_stat) != 0) {
            fsFsCreateDirectory(sdmc_fs, path);
        }
    }
    
    // Resolve LANG_AUTO and set normal.resolved_lang
    static void ResolveLangSetting(ConfigService &config_svc) {
        int resolved = config_svc.normal.lang;
        if (resolved == LANG_AUTO) {
            u64 lang_code = 0;
            SetLanguage lang = SetLanguage_ENUS;
            if (R_SUCCEEDED(setGetSystemLanguage(&lang_code)) &&
                R_SUCCEEDED(setMakeLanguage(lang_code, &lang))) {
                int idx = static_cast<int>(lang);
                if (idx >= 0 && idx <= 11) resolved = idx;
                else resolved = 1;  // English fallback
            } else {
                resolved = 1;  // English fallback
            }
        }
        config_svc.normal.resolved_lang = resolved;
    }
    
    int Save(ConfigService &config_svc, FileSystemService &fs_svc) {
        // Re-resolve language setting in case it changed (e.g., user selected a new language)
        ResolveLangSetting(config_svc);
        
        FsFileSystem *sdmc_fs = &fs_svc.devices[FileSystemSDMC];
        
        json_t *root = json_object();
        SetInt(root, "config_version", CONFIG_VERSION);
        
        // Normal mode settings
        json_t *normal = json_object();
        SetInt(normal, "language", config_svc.normal.lang);
        SetInt(normal, "dev_options", config_svc.normal.dev_options);
        SetInt(normal, "image_filename", config_svc.normal.image_filename);
        SetInt(normal, "enter_images_fullscreen", config_svc.normal.enter_images_fullscreen);
        SetInt(normal, "resolution_mode", config_svc.normal.resolution_mode);
        SetInt(normal, "theme_mode", config_svc.normal.theme_mode);
        SetInt(normal, "show_details", config_svc.normal.show_details);
        SetInt(normal, "show_stats", config_svc.normal.show_stats);
        SetString(normal, "last_device", config_svc.normal.last_device);
        SetString(normal, "last_cwd", config_svc.normal.last_cwd);
        
        json_t *accent_array = json_array();
        for (int i = 0; i < 3; i++)
            json_array_append_new(accent_array, json_real(config_svc.normal.accent_color[i]));
        json_object_set_new(normal, "accent_color", accent_array);
        
        SetInt(normal, "button_style", config_svc.normal.button_style);
        json_object_set_new(root, "normal", normal);
        
        // Applet mode settings (limited)
        json_t *applet = json_object();
        SetInt(applet, "dev_options", config_svc.applet.dev_options);
        SetString(applet, "last_device", config_svc.applet.last_device);
        SetString(applet, "last_cwd", config_svc.applet.last_cwd);
        json_object_set_new(root, "applet", applet);
        
        // Use unique_ptr with custom deleter for json_dumps result (freed with free())
        std::unique_ptr<char, decltype(&std::free)> buf(json_dumps(root, JSON_INDENT(2)), std::free);
        json_decref(root);
        
        if (!buf) return -1;
        
        u64 len = std::strlen(buf.get());
        fsFsDeleteFile(sdmc_fs, config_path);
        fsFsCreateFile(sdmc_fs, config_path, len, 0);
        
        FsFile file;
        Result ret;
        if (R_FAILED(ret = fsFsOpenFile(sdmc_fs, config_path, FsOpenMode_Write, std::addressof(file)))) {
            return ret;
        }
        
        if (R_FAILED(ret = fsFileWrite(std::addressof(file), 0, buf.get(), len, FsWriteOption_Flush))) {
            fsFileClose(std::addressof(file));
            return ret;
        }
        
        fsFileClose(std::addressof(file));
        return 0;
    }
    
    int Load(ConfigService &config_svc, FileSystemService &fs_svc, bool is_applet_mode) {
        FsFileSystem *sdmc_fs = &fs_svc.devices[FileSystemSDMC];
        
        config_svc.is_applet_mode = is_applet_mode;
        
        EnsureDirExists(sdmc_fs, "/switch/");
        EnsureDirExists(sdmc_fs, "/switch/NX-Shell/");
        
        // Check if file exists
        struct stat file_stat = { 0 };
        if (stat(config_path, &file_stat) != 0) {
            config_svc.normal = NormalConfig{};
            config_svc.applet = AppletConfig{};
            ResolveLangSetting(config_svc);
            return Save(config_svc, fs_svc);
        }
        
        FsFile file;
        Result ret;
        if (R_FAILED(ret = fsFsOpenFile(sdmc_fs, config_path, FsOpenMode_Read, std::addressof(file))))
            return ret;
        
        s64 size = 0;
        if (R_FAILED(ret = fsFileGetSize(std::addressof(file), std::addressof(size)))) {
            fsFileClose(std::addressof(file));
            return ret;
        }

        auto buf = std::make_unique<char[]>(size + 1);
        if (R_FAILED(ret = fsFileRead(std::addressof(file), 0, buf.get(), static_cast<u64>(size) + 1, FsReadOption_None, nullptr))) {
            fsFileClose(std::addressof(file));
            return ret;
        }
        
        fsFileClose(std::addressof(file));
        
        json_t *root;
        json_error_t error;
        root = json_loads(buf.get(), 0, std::addressof(error));
        
        if (!root) return -1;
        
        config_version_holder = GetInt(root, "config_version");
        if (config_version_holder < CONFIG_VERSION) {
            json_decref(root);
            fsFsDeleteFile(sdmc_fs, config_path);
            config_svc.normal = NormalConfig{};
            config_svc.applet = AppletConfig{};
            ResolveLangSetting(config_svc);
            return Save(config_svc, fs_svc);
        }

        // Load normal mode settings
        json_t *normal = json_object_get(root, "normal");
        if (normal && json_is_object(normal)) {
            config_svc.normal.lang = GetInt(normal, "language");
            config_svc.normal.dev_options = GetInt(normal, "dev_options");
            config_svc.normal.image_filename = GetInt(normal, "image_filename");
            config_svc.normal.enter_images_fullscreen = GetInt(normal, "enter_images_fullscreen");
            config_svc.normal.resolution_mode = GetInt(normal, "resolution_mode");
            config_svc.normal.theme_mode = GetInt(normal, "theme_mode");
            config_svc.normal.show_details = GetInt(normal, "show_details");
            config_svc.normal.show_stats = GetInt(normal, "show_stats");
            config_svc.normal.button_style = GetInt(normal, "button_style");
            config_svc.normal.last_device = GetString(normal, "last_device", "sdmc:");
            config_svc.normal.last_cwd = GetString(normal, "last_cwd", "/");

            json_t *accent_color = json_object_get(normal, "accent_color");
            if (accent_color && json_is_array(accent_color) && json_array_size(accent_color) == 3) {
                for (int i = 0; i < 3; i++)
                    config_svc.normal.accent_color[i] = static_cast<float>(json_real_value(json_array_get(accent_color, i)));
            }
        }
        
        // Load applet mode settings
        json_t *applet = json_object_get(root, "applet");
        if (applet && json_is_object(applet)) {
            config_svc.applet.dev_options = GetInt(applet, "dev_options");
            config_svc.applet.last_device = GetString(applet, "last_device", "sdmc:");
            config_svc.applet.last_cwd = GetString(applet, "last_cwd", "/");
        }

        json_decref(root);
        ResolveLangSetting(config_svc);
        return 0;
    }
    
    void ResetNormalConfig(ConfigService &config_svc) {
        config_svc.normal = NormalConfig{};
        ResolveLangSetting(config_svc);
    }
}
