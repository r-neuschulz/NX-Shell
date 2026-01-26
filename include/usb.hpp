#pragma once

#include <switch.h>
#include <vector>
#include "services.hpp"

namespace USB {
    Result Init(DeviceRegistry &dev_reg);
    void Exit(void);
    void Unmount(void);
    bool Connected(void);
}
