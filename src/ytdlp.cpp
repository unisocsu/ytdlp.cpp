#include "ytdlp/ytdlp.h"

#define YTDLP_VERSION_STRING "0.1.0"

extern "C" YTDLP_API const char* ytdlp_version(void) {
    return YTDLP_VERSION_STRING;
}

extern "C" YTDLP_API const char* ytdlp_build_info(void) {
#if defined(__ANDROID__)
    return "Android native build; libcurl HTTP(S) with mbedTLS backend";
#elif defined(_WIN32)
    return "Windows native build; libcurl HTTP(S) with mbedTLS backend";
#else
    return "Native C++ build; libcurl HTTP(S) with mbedTLS backend";
#endif
}
