#include "ytdlp/extractor.hpp"

#include "ytdlp/utils.hpp"
#include "ytdlp/html.hpp"
#include "ytdlp/network.hpp"

#include <algorithm>
#include <cctype>
#include <set>
#include <utility>

namespace ytdlp {
namespace extractor {
namespace {

const std::set<std::string>& direct_media_extensions() {
    static const std::set<std::string> extensions = {
        "3gp", "aac", "aiff", "alac", "avi", "flac", "flv", "m4a",
        "m4v", "mka", "mkv", "mov", "mp3", "mp4", "mpeg", "mpg",
        "oga", "ogg", "ogv", "opus", "ts", "wav", "webm", "wma", "wmv"
    };
    return extensions;
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool is_http_url(const std::string& url) {
    return url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0;
}

std::string remove_extension(const std::string& filename) {
    const auto dot = filename.rfind('.');
    if (dot == std::string::npos || dot == 0) return filename;
    return filename.substr(0, dot);
}

std::string media_id(const std::string& url) {
    std::string id = ytdlp::utils::url_basename(url);
    if (id.empty()) return url;
    return remove_extension(id);
}

}  // namespace

const char* DirectMediaExtractor::name() const {
    return "DirectMedia";
}

bool DirectMediaExtractor::suitable(const std::string& url) const {
    if (!is_http_url(url)) return false;
    const std::string ext = ytdlp::utils::determine_ext(url, "");
    return direct_media_extensions().find(lower(ext)) != direct_media_extensions().end();
}

ExtractResult DirectMediaExtractor::extract(const std::string& url) const {
    if (!suitable(url)) return {std::nullopt, "URL is not a supported direct media file"};
    const std::string ext = lower(ytdlp::utils::determine_ext(url, ""));
    MediaInfo info;
    info.id = media_id(url);
    info.title = info.id.empty() ? "video" : info.id;
    info.original_url = url;
    info.webpage_url = url;
    info.extractor_key = name();
    info.ext = ext;
    Format format;
    format.format_id = "direct";
    format.url = url;
    format.ext = ext;
    format.protocol = url.rfind("https://", 0) == 0 ? "https" : "http";
    info.formats.push_back(std::move(format));
    return {std::move(info), ""};
}

const char* GenericHtmlExtractor::name() const { return "GenericHTML"; }

bool GenericHtmlExtractor::suitable(const std::string& url) const {
    return is_http_url(url) && !DirectMediaExtractor().suitable(url);
}

ExtractResult GenericHtmlExtractor::extract(const std::string& url) const {
    if (!suitable(url)) return {std::nullopt, "URL is not a webpage URL"};
    ytdlp::network::HttpRequest request;
    request.url = url;
    request.method = "GET";
    request.timeout_seconds = 30;
    request.headers.emplace_back("User-Agent", "ytdlp.cpp/0.1 native media extractor");
    request.headers.emplace_back("Accept", "text/html,application/xhtml+xml,video/*;q=0.9,*/*;q=0.8");
    const auto response = ytdlp::network::perform(request);
    if (!response.ok()) {
        if (!response.error.empty()) return {std::nullopt, "Could not fetch webpage: " + response.error};
        return {std::nullopt, "Could not fetch webpage (HTTP " + std::to_string(response.status) + ")"};
    }
    const auto metadata = ytdlp::html::parse_metadata(response.body);
    std::string selected_url;
    std::string selected_ext;
    for (const auto& candidate : metadata.media_urls) {
        const auto joined = ytdlp::utils::url_join(std::optional<std::string>(url),
                                                   std::optional<std::string>(candidate));
        if (!joined || !is_http_url(*joined)) continue;
        const std::string ext = lower(ytdlp::utils::determine_ext(*joined, ""));
        if (direct_media_extensions().find(ext) == direct_media_extensions().end()) continue;
        selected_url = *joined;
        selected_ext = ext;
        break;
    }
    if (selected_url.empty()) return {std::nullopt,
        "Fetched the webpage, but found no direct media URL in metadata or HTML5 video/source tags. JavaScript and site-specific extractors are not supported yet."};
    MediaInfo info;
    info.id = media_id(selected_url);
    info.title = metadata.title.empty() ? info.id : metadata.title;
    if (info.title.empty()) info.title = "video";
    info.original_url = url;
    info.webpage_url = metadata.canonical_url.empty() ? url : metadata.canonical_url;
    info.extractor_key = name();
    info.ext = selected_ext;
    Format format;
    format.format_id = "html-direct";
    format.url = selected_url;
    format.ext = selected_ext;
    format.protocol = selected_url.rfind("https://", 0) == 0 ? "https" : "http";
    info.formats.push_back(std::move(format));
    return {std::move(info), ""};
}

ExtractorRegistry::ExtractorRegistry() {
    add(std::unique_ptr<InfoExtractor>(new DirectMediaExtractor()));
    add(std::unique_ptr<InfoExtractor>(new GenericHtmlExtractor()));
}

bool ExtractorRegistry::add(std::unique_ptr<InfoExtractor> extractor) {
    if (!extractor) return false;
    extractors_.push_back(std::move(extractor));
    return true;
}

const InfoExtractor* ExtractorRegistry::find(const std::string& url) const {
    for (const auto& extractor : extractors_)
        if (extractor && extractor->suitable(url)) return extractor.get();
    return nullptr;
}

ExtractResult ExtractorRegistry::extract(const std::string& url) const {
    const InfoExtractor* extractor = find(url);
    if (!extractor) return {std::nullopt, "No native extractor supports this URL"};
    return extractor->extract(url);
}

std::size_t ExtractorRegistry::size() const {
    return extractors_.size();
}

}  // namespace extractor
}  // namespace ytdlp
