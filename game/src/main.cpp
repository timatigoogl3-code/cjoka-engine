#include "SceneEditor.h"

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
extern "C" const char* __asan_default_options() {
    return "detect_leaks=0";
}
#endif
#endif

int main() {
    SceneEditor app;
    app.run();
    return 0;
}
