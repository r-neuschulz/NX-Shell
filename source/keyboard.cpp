#include <cstring>
#include <switch.h>

#include "config.hpp"
#include "keyboard.hpp"
#include "language.hpp"
#include "log.hpp"
#include "services.hpp"

namespace Keyboard {
    // Cached language index - set before showing keyboard since SDK callback signature is fixed
    static int s_cached_lang = 1;
    
    // Empty strings are invalid.
    // Note: SDK callback signature is fixed, so we use cached language value
    SwkbdTextCheckResult ValidateText(char *string, size_t size) {
        if (std::strcmp(string, "") == 0) {
            std::strncpy(string, strings[s_cached_lang][Lang::KeyboardEmpty], size); 
            return SwkbdTextCheckResult_Bad;
        }
        
        return SwkbdTextCheckResult_OK;
    }
    
    std::string GetText(ConfigService &config_svc, const std::string &guide_text, const std::string &initial_text) {
        // Cache language for the validation callback (SDK signature is fixed)
        s_cached_lang = config_svc.Lang();
        Result ret = 0;
        SwkbdConfig swkbd;
        static char input_string[256];
        
        if (R_FAILED(ret = swkbdCreate(std::addressof(swkbd), 0))) {
            Log::Error("swkbdCreate() failed: 0x%x\n", ret);
            swkbdClose(std::addressof(swkbd));
            return std::string();
        }
        
        swkbdConfigMakePresetDefault(std::addressof(swkbd));
        if (!guide_text.empty())
            swkbdConfigSetGuideText(std::addressof(swkbd), guide_text.c_str());
            
        if (!initial_text.empty())
            swkbdConfigSetInitialText(std::addressof(swkbd), initial_text.c_str());
            
        swkbdConfigSetTextCheckCallback(std::addressof(swkbd), Keyboard::ValidateText);
        if (R_FAILED(ret = swkbdShow(std::addressof(swkbd), input_string, sizeof(input_string)))) {
            Log::Error("swkbdShow() failed: 0x%x\n", ret);
            swkbdClose(std::addressof(swkbd));
            return std::string();
        }
        
        swkbdClose(std::addressof(swkbd));
        return input_string;
    }
}
