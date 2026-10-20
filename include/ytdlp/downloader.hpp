#ifndef YTDLP_CPP_DOWNLOADER_HPP
#define YTDLP_CPP_DOWNLOADER_HPP

#include "ytdlp/extractor.hpp"

#include <string>

namespace ytdlp {
namespace downloader {

struct DownloadOptions {
    std::string output_path;
    std::string ca_bundle_path;
    long timeout_seconds = 0; // 0 means no overall timeout
};

struct DownloadResult {
    bool success = false;
    long http_status = 0;
    std::string source_url;
    std::string output_path;
    std::string extractor;
    std::string error;
};

/*
 * First end-to-end native download pipeline:
 * URL -> extractor registry -> selected format -> TLS-verified HTTP download.
 * Supports direct media URLs and generic HTML pages that expose a direct media URL in metadata or HTML5 video/source tags.
 */
class DownloadEngine {
public:
    DownloadEngine() = default;

    DownloadResult download(const std::string& url,
                            const DownloadOptions& options) const;

private:
    extractor::ExtractorRegistry registry_;
};

} // namespace downloader
} // namespace ytdlp

#endif
