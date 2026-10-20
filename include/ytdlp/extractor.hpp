#ifndef YTDLP_CPP_EXTRACTOR_HPP
#define YTDLP_CPP_EXTRACTOR_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ytdlp {
namespace extractor {

/* Typed native equivalent of the common fields in yt-dlp's info dictionary. */
struct Format {
    std::string format_id;
    std::string url;
    std::string ext;
    std::string protocol;
    std::optional<std::int64_t> width;
    std::optional<std::int64_t> height;
    std::optional<double> tbr;
};

struct MediaInfo {
    std::string id;
    std::string title;
    std::string original_url;
    std::string webpage_url;
    std::string extractor_key;
    std::string ext;
    std::optional<double> duration;
    bool is_live = false;
    std::vector<Format> formats;
};

struct ExtractResult {
    std::optional<MediaInfo> info;
    std::string error;

    explicit operator bool() const { return info.has_value() && error.empty(); }
};

/* Base class corresponding to yt-dlp's InfoExtractor suitability/extraction split. */
class InfoExtractor {
public:
    virtual ~InfoExtractor() = default;
    virtual const char* name() const = 0;
    virtual bool suitable(const std::string& url) const = 0;
    virtual ExtractResult extract(const std::string& url) const = 0;
};

/* Recognizes URLs whose path itself is a media file. It does not parse HLS/DASH manifests. */
class DirectMediaExtractor final : public InfoExtractor {
public:
    const char* name() const override;
    bool suitable(const std::string& url) const override;
    ExtractResult extract(const std::string& url) const override;
};

/* Initial YouTube watch-page / Innertube extractor. Only direct URLs are supported;
 * signature deciphering and DASH audio/video merging are not implemented.
 */
class YouTubeExtractor final : public InfoExtractor {
public:
    const char* name() const override;
    bool suitable(const std::string& url) const override;
    ExtractResult extract(const std::string& url) const override;
};

/* Generic HTML-page extractor for pages exposing a direct media URL in Open Graph,
 * Twitter metadata, or HTML5 video/source tags. No JavaScript/site signatures.
 */
class GenericHtmlExtractor final : public InfoExtractor {
public:
    const char* name() const override;
    bool suitable(const std::string& url) const override;
    ExtractResult extract(const std::string& url) const override;
};

/* Ordered registry: the first registered suitable extractor wins, like yt-dlp's IE selection. */
class ExtractorRegistry {
public:
    ExtractorRegistry();
    bool add(std::unique_ptr<InfoExtractor> extractor);
    const InfoExtractor* find(const std::string& url) const;
    ExtractResult extract(const std::string& url) const;
    std::size_t size() const;

private:
    std::vector<std::unique_ptr<InfoExtractor>> extractors_;
};

}  // namespace extractor
}  // namespace ytdlp

#endif
