#ifndef YTDLP_CPP_HTTP_H
#define YTDLP_CPP_HTTP_H

#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

/* Download an HTTP(S) URL to a local file.
 *
 * Redirects are followed, but only HTTP and HTTPS URLs are permitted.
 * TLS peer and hostname verification are enabled. If ca_bundle_path is
 * non-null/non-empty, it is used as the PEM CA bundle.
 *
 * Returns 0 on success, non-zero on failure. If http_status is non-null,
 * it receives the final HTTP status code (or 0 if no response was received).
 * The error string from ytdlp_http_last_error() is thread-local.
 */
int ytdlp_http_download(const char* url,
                        const char* output_path,
                        const char* ca_bundle_path,
                        long timeout_seconds,
                        long* http_status);

/* Returns the last HTTP error for the calling thread. */
const char* ytdlp_http_last_error(void);

#if defined(__cplusplus)
}
#endif

#endif
