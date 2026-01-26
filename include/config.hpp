#pragma once

#include <string>
#include <switch.h>
#include "services.hpp"

namespace Config {
    // Load config from disk; sets is_applet_mode and resolves LANG_AUTO
    int Load(ConfigService &config_svc, FileSystemService &fs_svc, bool is_applet_mode);
    
    // Save config to disk
    int Save(ConfigService &config_svc, FileSystemService &fs_svc);
    
    // Reset normal config to defaults
    void ResetNormalConfig(ConfigService &config_svc);
}
