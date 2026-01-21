#pragma once

#include <string>

namespace Archive {
    void SetArchivePath(const std::string &path);
    const std::string& GetArchivePath(void);
    bool ExtractZip(void);
}
