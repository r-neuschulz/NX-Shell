#pragma once

#include <string>
#include "services.hpp"

namespace Keyboard {
    std::string GetText(ConfigService &config_svc, const std::string &guide_text, const std::string &initial_text);
}
