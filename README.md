# ytdlp.cpp

A native C++ porting project inspired by yt-dlp, starting with Android 4.4 KitKat (API 19) compatibility.

**Status: bootstrap stage.** This repository is not yet a downloader and does not yet implement modern TLS or yt-dlp extractors. The first goal is to establish a reproducible Android NDK build, then add a maintained TLS/HTTP stack and test it on API 19.

## Goals

- Native C++ implementation; do not launch Python or the original yt-dlp executable.
- Android API 19 compatibility, initially targeting `armeabi-v7a`.
- Keep networking behind a stable internal interface.
- Verify TLS certificates; never disable certificate verification as a compatibility workaround.
- Add site extractors incrementally and test behavior against the upstream project.
- Build using GitHub Actions, without requiring Android Studio on the developer's computer.

## Build

The GitHub Actions workflow builds the native shared library for Android API 19 and uploads it as an Actions artifact.

For local builds, use Android NDK r25c or another NDK verified to support API 19, with CMake and Ninja:

```sh
cmake -S . -B build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=armeabi-v7a \
  -DANDROID_PLATFORM=android-19
cmake --build build
```

## Planned milestones

1. **Build foundation** — API 19 compile and CI artifact.
2. **TLS and HTTP** — integrate a maintained TLS library and HTTP client, validate certificates, redirects, streaming downloads, timeouts, and error reporting on KitKat.
3. **Core utilities** — URL handling, JSON, formats, and downloader primitives.
4. **Extractor framework** — port extractors incrementally, starting with tests and a small number of sites.
5. **Compatibility tests** — compare metadata and download behavior with upstream yt-dlp.

A one-to-one port is a large project: Python behavior, extractor logic, and site-specific JavaScript dependencies all need deliberate equivalents in C++. Passing the Android build does not by itself mean a site is supported.

## Licensing

Before distributing a combined binary, review the licenses of yt-dlp and every dependency selected for the C++ implementation.
