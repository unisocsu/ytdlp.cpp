# Import redactions

The upstream reference snapshot had three credential-like values in yt_dlp/extractor/shahid.py that GitHub push protection blocked. Their values were replaced with REDACTED_UPSTREAM_CREDENTIAL so they are not copied into this repository. This means the upstream Shahid extractor in this reference snapshot is intentionally not usable as-is; native C++ porting must use a legitimate, non-secret configuration rather than restore these values.
