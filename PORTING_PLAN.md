# yt-dlp to native C++ porting plan

## Source snapshot
The unmodified upstream yt-dlp source tree is kept in base/.
The exact upstream commit is recorded in UPSTREAM_REVISION.txt.
Upstream source and its license notices must be preserved.

## Native implementation policy
- Do not execute or embed Python at runtime.
- Do not mechanically translate Python syntax without understanding behavior.
- Keep base/ as the upstream reference snapshot; implement C++ code in src/ and public interfaces in include/.
- Port behavior in small, testable modules and document each source-to-C++ mapping here.
- Add regression tests that compare supported inputs/outputs with upstream behavior.
- Never claim an extractor is ported until it has native tests.

## Initial mapping
| Upstream Python area | Native C++ destination | Status |
|---|---|---|
| yt_dlp/networking/ | src/network/ | Planned |
| yt_dlp/utils/ | src/utils/ | Planned |
| yt_dlp/extractor/common.py | src/extractor/ | Planned |
| yt_dlp/extractor/ site extractors | src/extractor/sites/ | Planned |
| yt_dlp/YoutubeDL.py | src/core/ | Planned |

## First porting milestone
1. Inspect upstream modules and dependencies.
2. Define C++ URL, request, response, cookie, and error types.
3. Add unit tests for URL parsing and HTTP behavior.
4. Port a small extractor end-to-end before expanding coverage.
