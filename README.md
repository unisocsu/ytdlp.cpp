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
- HTTP(S) download layer using libcurl, with TLS verification enabled, redirects restricted to HTTP(S), timeout support, and cleanup of partial files on failure.
- Initial end-to-end `DownloadEngine`: selects a supported extractor, chooses its first format, and downloads it to an explicit output path. Supports direct media-file URLs and generic HTML pages that expose direct media URLs through Open Graph/Twitter metadata or HTML5 video/source tags.
- Native request/response structures and a host/path/secure-aware in-memory cookie store.
- Initial native extractor framework: typed media/format results, ordered extractor registry, and direct media-file URL recognition.
- HTML metadata parser for page titles, descriptions, canonical links, Open Graph/Twitter video URLs, and HTML video/source tags; JavaScript is not executed.
- Windows x64 build workflow using MSVC and Windows Schannel TLS, producing `ytdlp.exe`, a DLL, import library, and public headers.
- Native utility ports: determine_ext, url_basename, int_or_none, str_or_none, urlencode_postdata, parse_duration, url_join, and update_url_query.
- Unit tests for URL helpers, duration parsing, cookies, and response headers.
- Host-side unit tests for utility behavior.
- Android API 19 / armeabi-v7a build workflow.

These are only the first building blocks. The generic HTML extractor fetches webpages and looks for direct media URLs in metadata and HTML5 video/source tags. It does not execute JavaScript, parse HLS/DASH manifests, or implement site-specific extraction, so typical modern YouTube pages and many other sites will still not work. This project is **not yet a complete yt-dlp replacement** and does not yet include native site extractors, full format selection, or media merging.

## Windows command-line usage

Download the `ytdlp-cpp-windows-x64` artifact from the successful [Windows x64 build](https://github.com/unisocsu/ytdlp.cpp/actions/workflows/windows-build.yml), extract the ZIP, and run PowerShell from the extracted folder. The executable and `ytdlp.dll` are both in `bin/`.

```powershell
.\\bin\\ytdlp.exe --version
.\\bin\\ytdlp.exe "https://example.com/video.mp4" -o "video.mp4"
.\\bin\\ytdlp.exe "https://example.com/video.mp4" -o "video.mp4" --timeout 300
```

Replace the example URL with a **direct media-file URL** that points to a downloadable file. A YouTube watch page or other webpage URL will not work yet. The command does not bypass access controls or provide site-specific extraction at this stage.

## Build and test

```sh
cmake -S . -B build-host -DYTDLP_BUILD_TESTS=ON -DYTDLP_BUILD_SHARED=OFF
cmake --build build-host --parallel 2
ctest --test-dir build-host --output-on-failure
```

The GitHub Actions workflows also build for Android API 19 and run host utility tests.

## Next porting steps

1. Stabilize generic HTML extraction and URL-resolution edge cases.
2. Add HLS manifest parsing and segmented downloading.
3. Port one site-specific extractor end-to-end and add regression tests.
4. Add DASH parsing and media merging.
5. Grow extractor coverage incrementally, tracking unsupported behavior explicitly.
