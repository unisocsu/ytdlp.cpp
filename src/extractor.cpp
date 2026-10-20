#include "ytdlp/extractor.hpp"

#include "ytdlp/utils.hpp"
#include "ytdlp/html.hpp"
#include "ytdlp/network.hpp"

#include <algorithm>
#include <cctype>
#include <regex>
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


std::string json_unescape(std::string value) {
    std::string out;
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] != '\\' || i + 1 >= value.size()) { out.push_back(value[i]); continue; }
        const char c = value[++i];
        if (c == 'n') out.push_back('\n');
        else if (c == 'r') out.push_back('\r');
        else if (c == 't') out.push_back('\t');
        else if (c == '"' || c == '\\' || c == '/') out.push_back(c);
        else if (c == 'u' && i + 4 < value.size()) {
            // YouTube commonly escapes URL separators as \u0026.
            unsigned int code = 0;
            bool valid = true;
            for (int j = 0; j < 4; ++j) {
                const char h = value[i + 1 + j];
                code <<= 4;
                if (h >= '0' && h <= '9') code += static_cast<unsigned int>(h - '0');
                else if (h >= 'a' && h <= 'f') code += static_cast<unsigned int>(h - 'a' + 10);
                else if (h >= 'A' && h <= 'F') code += static_cast<unsigned int>(h - 'A' + 10);
                else { valid = false; break; }
            }
            if (valid) {
                if (code < 0x80) out.push_back(static_cast<char>(code));
                else if (code < 0x800) {
                    out.push_back(static_cast<char>(0xC0 | (code >> 6)));
                    out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                } else {
                    out.push_back(static_cast<char>(0xE0 | (code >> 12)));
                    out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                    out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                }
                i += 4;
            } else out += "\\u";
        } else out.push_back(c);
    }
    return out;
}

std::string json_string_field(const std::string& json, const std::string& key) {
    const std::string marker = "\"" + key + "\"";
    std::size_t pos = json.find(marker);
    while (pos != std::string::npos) {
        pos += marker.size();
        while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
        if (pos >= json.size() || json[pos++] != ':') {
            pos = json.find(marker, pos);
            continue;
        }
        while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
        if (pos >= json.size() || json[pos++] != '"') {
            pos = json.find(marker, pos);
            continue;
        }
        const std::size_t begin = pos;
        bool escaped = false;
        for (; pos < json.size(); ++pos) {
            if (escaped) escaped = false;
            else if (json[pos] == '\\') escaped = true;
            else if (json[pos] == '"') return json_unescape(json.substr(begin, pos - begin));
        }
        return "";
    }
    return "";
}

std::string youtube_video_id(const std::string& url) {
    std::smatch match;
    static const std::regex patterns[] = {
        std::regex(R"([?&]v=([A-Za-z0-9_-]{11}))"),
        std::regex(R"(youtu\.be/([A-Za-z0-9_-]{11}))"),
        std::regex(R"(/(?:shorts|embed|live)/([A-Za-z0-9_-]{11}))")
    };
    for (const auto& pattern : patterns)
        if (std::regex_search(url, match, pattern)) return match[1].str();
    return "";
}

std::string youtube_api_key(const std::string& html) {
    static const std::regex key_pattern(R"KEY("INNERTUBE_API_KEY"\s*:\s*"([^"]+)")KEY");
    std::smatch match;
    return std::regex_search(html, match, key_pattern) ? match[1].str() : "";
}

std::vector<Format> youtube_direct_formats(const std::string& json) {
    std::vector<Format> formats;
    // Walk JSON objects while respecting quoted strings; format objects are small and
    // contain both mimeType and url. This deliberately avoids a heavyweight JSON dependency.
    for (std::size_t start = 0; start < json.size(); ++start) {
        if (json[start] != '{') continue;
        bool quoted = false, escaped = false;
        int depth = 0;
        std::size_t end = start;
        for (; end < json.size(); ++end) {
            const char c = json[end];
            if (quoted) {
                if (escaped) escaped = false;
                else if (c == '\\') escaped = true;
                else if (c == '"') quoted = false;
                continue;
            }
            if (c == '"') quoted = true;
            else if (c == '{') ++depth;
            else if (c == '}' && --depth == 0) { ++end; break; }
        }
        if (end > json.size() || depth != 0) continue;
        const std::string object = json.substr(start, end - start);
        const std::string url = json_string_field(object, "url");
        const std::string mime = json_string_field(object, "mimeType");
        if (url.empty() || mime.empty() || !is_http_url(url)) continue;
        Format format;
        format.url = url;
        format.protocol = "https";
        format.ext = mime.find("webm") != std::string::npos ? "webm" : "mp4";
        format.format_id = json_string_field(object, "itag");
        if (format.format_id.empty()) format.format_id = "youtube-direct";
        std::smatch height;
        if (std::regex_search(object, height, std::regex(R"("height"\s*:\s*(\d+))")))
            format.height = std::stoll(height[1].str());
        std::smatch bitrate;
        if (std::regex_search(object, bitrate, std::regex(R"("bitrate"\s*:\s*(\d+))")))
            format.tbr = std::stod(bitrate[1].str()) / 1000.0;
        formats.push_back(std::move(format));
    }
    return formats;
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


const char* YouTubeExtractor::name() const { return "YouTube"; }

bool YouTubeExtractor::suitable(const std::string& url) const {
    return url.find("youtube.com/") != std::string::npos ||
           url.find("youtube-nocookie.com/") != std::string::npos ||
           url.find("youtu.be/") != std::string::npos;
}

ExtractResult YouTubeExtractor::extract(const std::string& url) const {
    const std::string video_id = youtube_video_id(url);
    if (video_id.empty())
        return {std::nullopt, "Could not identify a YouTube video ID from this URL"};

    ytdlp::network::HttpRequest page_request;
    page_request.url = "https://www.youtube.com/watch?v=" + video_id;
    page_request.timeout_seconds = 30;
    page_request.headers.emplace_back("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/131.0 Safari/537.36");
    page_request.headers.emplace_back("Accept-Language", "en-US,en;q=0.8");
    const auto page = ytdlp::network::perform(page_request);
    if (!page.ok())
        return {std::nullopt, "Could not fetch YouTube watch page: " +
            (page.error.empty() ? "HTTP " + std::to_string(page.status) : page.error)};
    const std::string api_key = youtube_api_key(page.body);
    if (api_key.empty())
        return {std::nullopt, "YouTube did not expose an Innertube API key; page layout or access restrictions may have changed"};

    ytdlp::network::HttpRequest player_request;
    player_request.url = "https://www.youtube.com/youtubei/v1/player?key=" + api_key;
    player_request.method = "POST";
    player_request.timeout_seconds = 30;
    player_request.headers.emplace_back("Content-Type", "application/json");
    player_request.headers.emplace_back("Origin", "https://www.youtube.com");
    player_request.headers.emplace_back("User-Agent", "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 Chrome/131.0 Safari/537.36");
    player_request.body = "{\"videoId\":\"" + video_id +
        "\",\"context\":{\"client\":{\"clientName\":\"WEB\",\"clientVersion\":\"2.20250101.00.00\",\"hl\":\"en\",\"gl\":\"US\"}}}";
    const auto player = ytdlp::network::perform(player_request);
    if (!player.ok())
        return {std::nullopt, "YouTube player API request failed: " +
            (player.error.empty() ? "HTTP " + std::to_string(player.status) : player.error)};

    const std::string title = json_string_field(player.body, "title");
    auto formats = youtube_direct_formats(player.body);
    if (formats.empty()) {
        if (player.body.find("signatureCipher") != std::string::npos ||
            player.body.find("cipher") != std::string::npos) {
            return {std::nullopt, "YouTube returned ciphered media URLs. Signature deciphering and JavaScript challenge support are not implemented yet."};
        }
        return {std::nullopt, "YouTube returned no direct media URLs. This video may require signature deciphering, a different client, or may be unavailable."};
    }
    // Prefer a combined audio/video format where available; otherwise use the
    // highest-resolution direct format. Separate DASH tracks are not merged yet.
    std::stable_sort(formats.begin(), formats.end(), [](const Format& a, const Format& b) {
        return a.height.value_or(0) > b.height.value_or(0);
    });
    MediaInfo info;
    info.id = video_id;
    info.title = title.empty() ? video_id : title;
    info.original_url = url;
    info.webpage_url = "https://www.youtube.com/watch?v=" + video_id;
    info.extractor_key = name();
    info.ext = formats.front().ext;
    info.formats = std::move(formats);
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
    add(std::unique_ptr<InfoExtractor>(new YouTubeExtractor()));
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
