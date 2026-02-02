#pragma once

#include <string>
#include "services.hpp"

namespace Archive {
    void SetArchivePath(FileSystemService &fs_svc, const std::string &path);
    const std::string& GetArchivePath(void);
    bool Extract(App &app);
}
