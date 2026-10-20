# ytdlp.cpp

A native C++ porting project targeting older Android devices, beginning with Android 4.4 (API 19).

## Repository layout

- **base/** — upstream yt-dlp source snapshot used as the behavior reference. The imported upstream revision is recorded in [UPSTREAM_REVISION.txt](UPSTREAM_REVISION.txt).
- **src/** and **include/** — native C++ implementation. Python code is not executed by the native library.
- **PORTING_PLAN.md** — module-by-module migration plan and status.
- **tests/** — native regression tests.

The source snapshot is imported from https://github.com/yt-dlp/yt-dlp. Its upstream license notices are retained under base/. A few credential-like values in the upstream Shahid extractor were redacted because GitHub push protection blocked importing them; see base/IMPORT_REDACTIONS.md.

## Native code implemented so far

- C API for version/build information.
- HTTP(S) download layer using libcurl, with TLS verification enabled.
- Native request/response structures and a host/path/secure-aware in-memory cookie store.
- Initial native extractor framework: typed media/format results, ordered extractor registry, and direct media-file URL recognition.
- Native utility ports: determine_ext, url_basename, int_or_none, str_or_none, urlencode_postdata, parse_duration, url_join, and update_url_query.
- Unit tests for URL helpers, duration parsing, cookies, and response headers.
- Host-side unit tests for utility behavior.
- Android API 19 / armeabi-v7a build workflow.

These are only the first building blocks. The direct-media extractor handles URLs whose paths are media files; it does not parse HLS/DASH manifests or implement site-specific extraction. This project is **not yet a complete yt-dlp replacement** and does not yet include native site extractors, full format selection, or media merging.

## Build and test

```sh
cmake -S . -B build-host -DYTDLP_BUILD_TESTS=ON -DYTDLP_BUILD_SHARED=OFF
cmake --build build-host --parallel 2
ctest --test-dir build-host --output-on-failure
```

The GitHub Actions workflows also build for Android API 19 and run host utility tests.

## Next porting steps

1. Stabilize utility and cookie behavior and edge cases.
2. Expand the common extractor framework and format-selection metadata.
3. Port one site extractor end-to-end and add regression tests.
4. Add HLS/DASH manifest parsing and segmented downloading.
5. Grow extractor coverage incrementally, tracking unsupported behavior explicitly.
