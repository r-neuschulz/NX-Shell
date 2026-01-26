#pragma once

#include "services.hpp"

namespace Log {
    void Init(App &app);
    void Error(const char *data, ...);
    void Debug(const char *data, ...);
    void Flush(void);  // Force flush all log output (useful before potential crashes)
    void Exit(void);
}
