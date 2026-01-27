#pragma once

#include <string>
#include "services.hpp"

namespace Net {
    // Socket initialization management (lazy init for faster startup)
    void InitSocketWithNxlink(void); // Init with nxlink stdio (for dev_options mode)
    void EnsureSocketReady(void);    // Lazy init when network is needed (no nxlink)
    bool IsSocketInitialized(void);  // Check if socket was initialized
    void ExitSocket(void);           // Cleanup socket on exit
    
    bool GetNetworkStatus(void);
    bool GetAvailableUpdate(const std::string &tag);
    std::string GetLatestReleaseJSON(void);
    std::string GetLatestReleaseNRO(FileSystemService &fs_svc, const std::string &tag);
}
