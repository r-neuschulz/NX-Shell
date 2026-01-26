#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <jansson.h>

#include "config.hpp"
#include "gui.hpp"
#include "log.hpp"
#include "services.hpp"

#define CONFIG_VERSION 16

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
    
    // Resolve LANG_AUTO to actual language index
    static int ResolveLang(int lang_setting) {
        if (lang_setting != LANG_AUTO)
            return lang_setting;
        
        u64 lang_code = 0;
        SetLanguage lang = SetLanguage_ENUS;
        if (R_SUCCEEDED(setGetSystemLanguage(&lang_code)) &&
            R_SUCCEEDED(setMakeLanguage(lang_code, &lang))) {
            int idx = static_cast<int>(lang);
            if (idx >= 0 && idx <= 11) return idx;
        }
        return 1;  // English fallback
    }
    
    void UpdateEffective(ConfigService &config_svc, bool is_applet_mode) {
        if (is_applet_mode) {
            // Applet mode: forced defaults, only logging and navigation from saved
            config_svc.effective.lang = 1;  // English
            config_svc.effective.dev_options = config_svc.saved.applet_dev_options;
            config_svc.effective.image_filename = false;
            config_svc.effective.enter_images_fullscreen = false;
            config_svc.effective.resolution_mode = ResolutionMode_720p;
            config_svc.effective.theme_mode = ThemeMode_Dark;
            config_svc.effective.show_details = false;
            config_svc.effective.show_stats = false;
            config_svc.effective.last_device = config_svc.saved.applet_last_device;
            config_svc.effective.last_cwd = config_svc.saved.applet_last_cwd;
            config_svc.effective.accent_color[0] = 0.0f;
            config_svc.effective.accent_color[1] = 0.50f;
            config_svc.effective.accent_color[2] = 0.50f;
            config_svc.effective.button_style = ButtonStyle_Mono;
        } else {
            // Title mode: use saved config (resolve LANG_AUTO)
            config_svc.effective = config_svc.saved;
            config_svc.effective.lang = ResolveLang(config_svc.saved.lang);
            config_svc.effective.last_device = config_svc.saved.last_device;
            config_svc.effective.last_cwd = config_svc.saved.last_cwd;
        }
    }
    
    int Save(ConfigService &config_svc, FileSystemService &fs_svc) {
        ConfigData &config = config_svc.saved;
        FsFileSystem *sdmc_fs = &fs_svc.devices[FileSystemSDMC];
        
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
        
        json_t *accent_array = json_array();
        for (int i = 0; i < 3; i++)
            json_array_append_new(accent_array, json_real(config.accent_color[i]));
        json_object_set_new(root, "accent_color", accent_array);
        
        SetInt(root, "button_style", config.button_style);
        SetInt(root, "applet_dev_options", config.applet_dev_options);
        SetString(root, "applet_last_device", config.applet_last_device);
        SetString(root, "applet_last_cwd", config.applet_last_cwd);
        
        char *buf = json_dumps(root, JSON_INDENT(1));
        json_decref(root);
        
        if (!buf) return -1;
        
        u64 len = std::strlen(buf);
        fsFsDeleteFile(sdmc_fs, config_path);
        fsFsCreateFile(sdmc_fs, config_path, len, 0);
        
        FsFile file;
        Result ret;
        if (R_FAILED(ret = fsFsOpenFile(sdmc_fs, config_path, FsOpenMode_Write, std::addressof(file)))) {
            free(buf);
            return ret;
        }
        
        if (R_FAILED(ret = fsFileWrite(std::addressof(file), 0, buf, len, FsWriteOption_Flush))) {
            free(buf);
            fsFileClose(std::addressof(file));
            return ret;
        }
        
        fsFileClose(std::addressof(file));
        free(buf);
        
        UpdateEffective(config_svc, GUI::IsAppletMode());
        return 0;
    }
    
    int Load(ConfigService &config_svc, FileSystemService &fs_svc) {
        FsFileSystem *sdmc_fs = &fs_svc.devices[FileSystemSDMC];
        
        EnsureDirExists(sdmc_fs, "/switch/");
        EnsureDirExists(sdmc_fs, "/switch/NX-Shell/");
        
        // Check if file exists
        struct stat file_stat = { 0 };
        if (stat(config_path, &file_stat) != 0) {
            config_svc.saved = ConfigData{};
            UpdateEffective(config_svc, GUI::IsAppletMode());
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
        
        if (!root) return -1;
        
        config_version_holder = GetInt(root, "config_version");
        if (config_version_holder < CONFIG_VERSION) {
            json_decref(root);
            fsFsDeleteFile(sdmc_fs, config_path);
            config_svc.saved = ConfigData{};
            UpdateEffective(config_svc, GUI::IsAppletMode());
            return Save(config_svc, fs_svc);
        }

        ConfigData &config = config_svc.saved;
        config.lang = GetInt(root, "language");
        config.dev_options = GetInt(root, "dev_options");
        config.image_filename = GetInt(root, "image_filename");
        config.enter_images_fullscreen = GetInt(root, "enter_images_fullscreen");
        config.resolution_mode = GetInt(root, "resolution_mode");
        config.theme_mode = GetInt(root, "theme_mode");
        config.show_details = GetInt(root, "show_details");
        config.show_stats = GetInt(root, "show_stats");
        config.button_style = GetInt(root, "button_style");
        config.last_device = GetString(root, "last_device", "sdmc:");
        config.last_cwd = GetString(root, "last_cwd", "/");

        json_t *accent_color = json_object_get(root, "accent_color");
        if (accent_color && json_is_array(accent_color) && json_array_size(accent_color) == 3) {
            for (int i = 0; i < 3; i++)
                config.accent_color[i] = static_cast<float>(json_real_value(json_array_get(accent_color, i)));
        }

        config.applet_dev_options = GetInt(root, "applet_dev_options");
        config.applet_last_device = GetString(root, "applet_last_device", "sdmc:");
        config.applet_last_cwd = GetString(root, "applet_last_cwd", "/");

        json_decref(root);
        UpdateEffective(config_svc, GUI::IsAppletMode());
        return 0;
    }
    
    int GetLang(ConfigService &config_svc) {
        return config_svc.effective.lang;
    }
    
    void SetLastDevice(ConfigService &config_svc, const std::string& dev, bool is_applet_mode) {
        if (is_applet_mode) {
            config_svc.saved.applet_last_device = dev;
            config_svc.effective.last_device = dev;
        } else {
            config_svc.saved.last_device = dev;
            config_svc.effective.last_device = dev;
        }
    }
    
    void SetLastCwd(ConfigService &config_svc, const std::string& path, bool is_applet_mode) {
        if (is_applet_mode) {
            config_svc.saved.applet_last_cwd = path;
            config_svc.effective.last_cwd = path;
        } else {
            config_svc.saved.last_cwd = path;
            config_svc.effective.last_cwd = path;
        }
    }
}
