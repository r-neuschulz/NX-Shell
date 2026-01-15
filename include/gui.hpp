#pragma once

namespace GUI {
    // Display dimensions (changes based on docked/handheld mode)
    extern int display_width;
    extern int display_height;
    
    bool Init(void);
    bool SwapBuffers(void);
    bool Loop(u64 &key);
    void Render(void);
    void Exit(void);
    
    // Returns true if console is docked (1080p), false if handheld (720p)
    bool IsDocked(void);
    
    // Updates display dimensions based on current dock state
    void UpdateDisplayDimensions(void);
}
