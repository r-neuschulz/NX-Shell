#pragma once

#include <string>
#include <switch.h>

// Resolution modes for display output
enum ResolutionMode {
    ResolutionMode_Auto = 0,  // Auto-detect based on dock state
    ResolutionMode_1080p = 1, // Force 1080p
    ResolutionMode_720p = 2   // Force 720p
};

typedef struct {
    int lang = 1;
    bool dev_options = false;
    bool image_filename = false;
    bool multi_lang = false;
    int resolution_mode = ResolutionMode_Auto;
} config_t;

extern config_t cfg;
extern std::string cwd;
extern std::string device;

namespace Config {
    int Save(config_t &config);
    int Load(void);
}
