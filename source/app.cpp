#include "services.hpp"

// Global App instance
static App s_app;
App* App::instance = &s_app;

App& GetApp() {
    return s_app;
}
