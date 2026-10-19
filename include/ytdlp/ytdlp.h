#ifndef YTDLP_CPP_YTDLP_H
#define YTDLP_CPP_YTDLP_H

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(_WIN32)
  #if defined(YTDLP_CPP_BUILDING)
    #define YTDLP_API __declspec(dllexport)
  #else
    #define YTDLP_API __declspec(dllimport)
  #endif
#else
  #define YTDLP_API __attribute__((visibility("default")))
#endif

/* Returns the library version, for example "0.1.0". */
YTDLP_API const char* ytdlp_version(void);

/* Returns a short description of the build target. */
YTDLP_API const char* ytdlp_build_info(void);

#if defined(__cplusplus)
}
#endif

#endif /* YTDLP_CPP_YTDLP_H */
