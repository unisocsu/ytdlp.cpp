#include "ytdlp/ytdlp.h"

#define YTDLP_VERSION_STRING "0.1.0"

extern "C" YTDLP_API const char* ytdlp_version(void) {
    return YTDLP_VERSION_STRING;
}

extern "C" YTDLP_API const char* ytdlp_build_info(void) {
#if defined(__ANDROID__)
    return "Android native build; networking/TLS not integrated yet";
#elif defined(_WIN32)
    return "Windows native build; networking/TLS not integrated yet";
#else
    return "Native C++ build; networking/TLS not integrated yet";
#endif
}
