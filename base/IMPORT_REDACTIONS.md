# Import redactions

GitHub push protection flagged three credential-like values in yt_dlp/extractor/shahid.py. Their values were replaced with REDACTED_UPSTREAM_CREDENTIAL so they are not copied into this repository. The upstream Shahid extractor in this reference snapshot is intentionally not usable as-is; native C++ porting must use legitimate, non-secret configuration.
