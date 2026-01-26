#pragma once

#include <glad/glad.h>
#include <switch.h>
#include <vector>
#include <string>
#include "services.hpp"

namespace Textures {
    bool LoadImageFile(const std::string &path, std::vector<Tex> &textures);
    void Free(Tex &texture);
    void Init(App &app);
    void Exit(App &app);
}
