#pragma once

#include <string>
#include <switch.h>
#include "services.hpp"

// Legacy typedef for compatibility
typedef ConfigData config_t;

namespace Config {
    // Primary API - takes service references
    int Save(ConfigService &config_svc, FileSystemService &fs_svc);
    int Load(ConfigService &config_svc, FileSystemService &fs_svc);
    void UpdateEffective(ConfigService &config_svc, bool is_applet_mode);
    int GetLang(ConfigService &config_svc);
    void SetLastDevice(ConfigService &config_svc, const std::string& device, bool is_applet_mode);
    void SetLastCwd(ConfigService &config_svc, const std::string& cwd, bool is_applet_mode);
    
    // Legacy API - uses global App instance (defined in legacy.cpp)
    int Save(config_t &config);
    int Load(void);
    void UpdateEffective(void);
    int GetLang(void);
    void SetLastDevice(const std::string& device);
    void SetLastCwd(const std::string& cwd);
}
