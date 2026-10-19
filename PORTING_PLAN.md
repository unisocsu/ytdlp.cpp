# yt-dlp to native C++ porting plan

## Source snapshot

The upstream yt-dlp source tree is kept in base/. The exact upstream commit is recorded in UPSTREAM_REVISION.txt, and the import workflow can refresh the snapshot. Upstream license notices are retained. Credential-like values blocked by GitHub push protection are redacted and documented in base/IMPORT_REDACTIONS.md.

## Native implementation policy

- Do not execute or embed Python at runtime.
- Do not mechanically translate Python syntax without understanding behavior.
- Keep base/ as the upstream reference snapshot; implement C++ code in src/ and public interfaces in include/.
- Port behavior in small, testable modules and document each source-to-C++ mapping here.
- Add regression tests that compare supported inputs/outputs with upstream behavior.
- Never claim an extractor is ported until it has native tests.

## Source-to-C++ mapping

| Upstream Python area | Native C++ destination | Status |
|---|---|---|
| yt_dlp/utils/_utils.py: determine_ext | src/utils.cpp: determine_ext | Initial implementation; common extensions only |
| yt_dlp/utils/_utils.py: url_basename | src/utils.cpp: url_basename | Initial implementation; tests added |
| yt_dlp/utils/_utils.py: int_or_none | src/utils.cpp: int_or_none | Initial implementation; numeric strings only |
| yt_dlp/utils/_utils.py: str_or_none | src/utils.cpp: str_or_none | Initial implementation for optional strings |
| yt_dlp/utils/_utils.py: urlencode_postdata | src/utils.cpp: urlencode_postdata | Initial implementation for string key/value pairs |
| yt_dlp/networking/ | src/network/ | Planned |
| yt_dlp/extractor/common.py | src/extractor/ | Planned |
| yt_dlp/extractor/ site extractors | src/extractor/sites/ | Planned |
| yt_dlp/YoutubeDL.py | src/core/ | Planned |

## Validation

- Host CI compiles and runs tests for the initial utility ports.
- Android API 19 build remains a separate validation target; do not assume it works until its workflow succeeds.
- Utility behavior is not yet fully equivalent to Python for arbitrary objects, all extension names, or every edge case.

## Next milestones

1. Expand utility parity: duration parsing, URL joining, query updates, and robust integer scaling.
2. Define native URL, request, response, cookie, and error types on top of the HTTP layer.
3. Port the common extractor framework.
4. Port one site extractor end-to-end and add regression tests.
5. Grow extractor coverage incrementally, tracking unsupported behavior explicitly.
