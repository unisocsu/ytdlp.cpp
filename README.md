# ytdlp.cpp

A native C++ project aiming to port yt-dlp functionality to older Android devices, starting with Android 4.4 (API 19).

> **Current status:** the project now has a native HTTP(S) download layer built around libcurl and mbedTLS. This is an early foundation, not yet a complete yt-dlp replacement: site extractors, format selection, manifests, and media post-processing still need to be ported.

## Current features

- C API suitable for use from Java/JNI or other native callers.
- HTTP and HTTPS download to a file.
- Redirect following, limited to HTTP/HTTPS protocols.
- TLS certificate-chain and hostname verification enabled by default.
- Optional caller-supplied PEM CA bundle path.
- Android API 19 / `armeabi-v7a` CI build using the Android NDK.
- TLS is built into the native dependency stack; this does not require replacing Android's system TLS implementation or rooting the device.

## HTTP download API

Include `ytdlp/ytdlp.h` and `ytdlp/http.h`, then call:

```c
#include "ytdlp/http.h"

long status = 0;
int result = ytdlp_http_download(
    "https://example.com/file.bin",
    "/data/local/tmp/file.bin",
    "/path/to/cacert.pem", /* pass NULL to use libcurl's configured defaults */
    120,
    &status
);

if (result != 0) {
    /* Copy or log ytdlp_http_last_error() before another call on this thread. */
}
```

The destination's parent directory must already exist and be writable. A failed transfer removes the partial output file. The CA bundle should be packaged by the application or supplied by the caller; do not disable TLS verification to work around certificate errors.

## Build

The GitHub Actions workflow builds mbedTLS and libcurl from pinned release tags for Android API 19, then links them into `libytdlp.so`. Run it from the Actions tab or push to `main`.

The resulting artifact is named `ytdlp-cpp-android-api19-armeabi-v7a`.

## Roadmap

1. **Native transport:** HTTP(S), redirects, timeouts, verified TLS. *(in progress; CI validation required)*
2. Common URL, header, cookie, and response utilities.
3. Extractor framework and JSON/HTML helpers.
4. Port site extractors incrementally, with regression tests against yt-dlp.
5. Format selection, HLS/DASH handling, and media merging where feasible.

## Important scope note

A true one-to-one port of yt-dlp is a large project. This repository does not yet support arbitrary video sites. The immediate goal is to establish a reliable native transport layer, then add extraction features in testable increments.
