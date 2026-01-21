#pragma once

namespace Log {
    void Init(void);
    void Error(const char *data, ...);
    void Debug(const char *data, ...);
    void Flush(void);  // Force flush all log output (useful before potential crashes)
    void Exit(void);
}
