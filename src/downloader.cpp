#include "ytdlp/downloader.hpp"

#include "ytdlp/http.h"

#include <utility>

namespace ytdlp {
namespace downloader {

DownloadResult DownloadEngine::download(const std::string& url,
                                        const DownloadOptions& options) const {
    DownloadResult result;
    result.source_url = url;
    result.output_path = options.output_path;

    if (options.output_path.empty()) {
        result.error = "Output path must not be empty";
        return result;
    }

    const extractor::ExtractResult extracted = registry_.extract(url);
    if (!extracted) {
        result.error = extracted.error.empty() ? "Could not extract a downloadable format"
                                               : extracted.error;
        return result;
    }

    const extractor::MediaInfo& info = *extracted.info;
    if (info.formats.empty()) {
        result.error = "Extractor returned no downloadable formats";
        return result;
    }

    // Until format selection is implemented, use the first format in extractor order.
    const extractor::Format& format = info.formats.front();
    result.extractor = info.extractor_key;
    result.source_url = format.url;

    const int code = ytdlp_http_download(
        format.url.c_str(),
        options.output_path.c_str(),
        options.ca_bundle_path.empty() ? nullptr : options.ca_bundle_path.c_str(),
        options.timeout_seconds,
        &result.http_status);

    if (code != 0) {
        const char* message = ytdlp_http_last_error();
        result.error = message ? message : "HTTP download failed";
        return result;
    }

    result.success = true;
    return result;
}

} // namespace downloader
} // namespace ytdlp
