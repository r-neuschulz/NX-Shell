#pragma once

#include <glad/glad.h>
#include <switch.h>
#include <vector>
#include <string>
#include "services.hpp"

// Legacy extern declarations - forward to App instance
extern std::vector<Tex>& file_icons;
extern Tex& folder_icon;
extern Tex& check_icon;
extern Tex& uncheck_icon;
extern Tex& partcheck_icon;
extern Tex& drive_icon;
extern Tex& settings_icon;

namespace Textures {
    bool LoadImageFile(const std::string &path, std::vector<Tex> &textures);
    void Free(Tex &texture);
    void Init(void);
    void Exit(void);
}
