#include <cstdio>
#include <cstring>
#include <jansson.h>

#include "config.hpp"
#include "fs.hpp"
#include "gui.hpp"
#include "log.hpp"

#define CONFIG_VERSION 16

config_t cfg;
config_t eff;  // Effective config - read from this!

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
    
    static inline void EnsureDirExists(const char *path) {
        if (!FS::DirExists(path))
            fsFsCreateDirectory(std::addressof(devices[FileSystemSDMC]), path);
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
    
    void UpdateEffective(void) {
        if (GUI::IsAppletMode()) {
            // Applet mode: forced defaults, only logging and navigation from cfg
            eff.lang = 1;  // English
            eff.dev_options = cfg.applet_dev_options;
            eff.image_filename = false;
            eff.enter_images_fullscreen = false;
            eff.resolution_mode = ResolutionMode_720p;
            eff.theme_mode = ThemeMode_Dark;
            eff.show_details = false;
            eff.show_stats = false;
            eff.last_device = cfg.applet_last_device;
            eff.last_cwd = cfg.applet_last_cwd;
            eff.accent_color[0] = 0.0f;
            eff.accent_color[1] = 0.50f;
            eff.accent_color[2] = 0.50f;
            eff.button_style = ButtonStyle_Mono;
        } else {
            // Title mode: use saved config (resolve LANG_AUTO)
            eff = cfg;
            eff.lang = ResolveLang(cfg.lang);
            eff.last_device = cfg.last_device;
            eff.last_cwd = cfg.last_cwd;
        }
    }
    
    int Save(config_t &config) {
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
        fsFsDeleteFile(std::addressof(devices[FileSystemSDMC]), config_path);
        fsFsCreateFile(std::addressof(devices[FileSystemSDMC]), config_path, len, 0);
        
        FsFile file;
        Result ret;
        if (R_FAILED(ret = fsFsOpenFile(std::addressof(devices[FileSystemSDMC]), config_path, FsOpenMode_Write, std::addressof(file)))) {
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
        
        UpdateEffective();  // Keep eff in sync
        return 0;
    }
    
    int Load(void) {
        EnsureDirExists("/switch/");
        EnsureDirExists("/switch/NX-Shell/");
            
        if (!FS::FileExists(config_path)) {
            cfg = {};
            UpdateEffective();
            return Config::Save(cfg);
        }
        
        FsFile file;
        Result ret;
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
        
        if (!root) return -1;
        
        config_version_holder = GetInt(root, "config_version");
        if (config_version_holder < CONFIG_VERSION) {
            json_decref(root);
            fsFsDeleteFile(std::addressof(devices[FileSystemSDMC]), config_path);
            cfg = {};
            UpdateEffective();
            return Config::Save(cfg);
        }

        cfg.lang = GetInt(root, "language");
        cfg.dev_options = GetInt(root, "dev_options");
        cfg.image_filename = GetInt(root, "image_filename");
        cfg.enter_images_fullscreen = GetInt(root, "enter_images_fullscreen");
        cfg.resolution_mode = GetInt(root, "resolution_mode");
        cfg.theme_mode = GetInt(root, "theme_mode");
        cfg.show_details = GetInt(root, "show_details");
        cfg.show_stats = GetInt(root, "show_stats");
        cfg.button_style = GetInt(root, "button_style");
        cfg.last_device = GetString(root, "last_device", "sdmc:");
        cfg.last_cwd = GetString(root, "last_cwd", "/");

        json_t *accent_color = json_object_get(root, "accent_color");
        if (accent_color && json_is_array(accent_color) && json_array_size(accent_color) == 3) {
            for (int i = 0; i < 3; i++)
                cfg.accent_color[i] = static_cast<float>(json_real_value(json_array_get(accent_color, i)));
        }

        cfg.applet_dev_options = GetInt(root, "applet_dev_options");
        cfg.applet_last_device = GetString(root, "applet_last_device", "sdmc:");
        cfg.applet_last_cwd = GetString(root, "applet_last_cwd", "/");

        json_decref(root);
        UpdateEffective();
        return 0;
    }
    
    int GetLang(void) {
        return eff.lang;
    }
    
    void SetLastDevice(const std::string& dev) {
        if (GUI::IsAppletMode()) {
            cfg.applet_last_device = dev;
            eff.last_device = dev;
        } else {
            cfg.last_device = dev;
            eff.last_device = dev;
        }
    }
    
    void SetLastCwd(const std::string& path) {
        if (GUI::IsAppletMode()) {
            cfg.applet_last_cwd = path;
            eff.last_cwd = path;
        } else {
            cfg.last_cwd = path;
            eff.last_cwd = path;
        }
    }
}
