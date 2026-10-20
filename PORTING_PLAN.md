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
| yt_dlp/utils/_utils.py: determine_ext | src/utils.cpp: determine_ext | Initial implementation; common filename extensions |
| yt_dlp/utils/_utils.py: url_basename | src/utils.cpp: url_basename | Implemented and tested |
| yt_dlp/utils/_utils.py: int_or_none | src/utils.cpp: int_or_none | Numeric strings supported; scaling edge cases still need review |
| yt_dlp/utils/_utils.py: str_or_none | src/utils.cpp: str_or_none | Optional string pass-through |
| yt_dlp/utils/_utils.py: urlencode_postdata | src/utils.cpp: urlencode_postdata | String key/value form encoding |
| yt_dlp/utils/_utils.py: parse_duration | src/utils.cpp: parse_duration | Clock, common unit strings, and day/time ISO-8601 subset |
| urllib.parse.urljoin usage | src/utils.cpp: url_join | Common absolute/relative URL joining; not a full RFC implementation |
| URL query mutation helpers | src/utils.cpp: update_url_query | Replaces supplied keys, preserves other fields and fragment |
| yt_dlp/networking/ request/response concepts | include/ytdlp/network.hpp, src/network.cpp | Initial request execution, response headers/body, and scoped cookie store; advanced proxy/retry/impersonation support not ported |
| yt_dlp/extractor/common.py: InfoExtractor selection/result basics | include/ytdlp/extractor.hpp, src/extractor.cpp | Native typed MediaInfo/Format, ordered registry, and direct-file URL extractor; no site extractors or manifest parsing yet |

## Validation

- Host CI compiles and runs tests for the native utility ports.
- Android API 19 build is a separate validation target; do not assume it works until its workflow succeeds.
- Utility behavior is not yet fully equivalent to Python for arbitrary objects, every duration syntax, URL edge cases, or all extension names.

## Next milestones

1. Expand networking parity: proxy configuration, retry policy, redirect handling details, and cookie expiry/domain edge cases.
2. Expand the native common extractor framework (URL matching, suitability, result dictionaries as typed C++ structures); initial registry and direct-media handling now exist.
3. Port one site extractor end-to-end and add regression tests; direct-file handling is only a baseline, not a site extractor.
4. Add format selection and media metadata parsing.
5. Grow extractor coverage incrementally, tracking unsupported behavior explicitly.
