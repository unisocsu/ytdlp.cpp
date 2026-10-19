#ifndef YTDLP_CPP_UTILS_HPP
#define YTDLP_CPP_UTILS_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ytdlp {
namespace utils {

/* Initial native ports of small helpers from base/yt_dlp/utils/_utils.py. */
std::string determine_ext(
    const std::optional<std::string>& url,
    const std::string& default_ext = "unknown_video");
std::string url_basename(const std::string& url);
std::optional<std::int64_t> int_or_none(
    const std::optional<std::string>& value,
    std::int64_t scale = 1,
    std::int64_t invscale = 1,
    std::optional<int> base = std::nullopt);
std::optional<std::string> str_or_none(
    const std::optional<std::string>& value);
std::string urlencode_postdata(
    const std::vector<std::pair<std::string, std::string>>& fields);

/* Returns seconds for supported clock, unit, and ISO-8601 duration forms. */
std::optional<double> parse_duration(const std::optional<std::string>& value);

/* URL helpers for absolute and relative HTTP-style URLs. */
std::optional<std::string> url_join(
    const std::optional<std::string>& base,
    const std::optional<std::string>& relative);
std::string update_url_query(
    const std::string& url,
    const std::vector<std::pair<std::string, std::string>>& updates);

}  // namespace utils
}  // namespace ytdlp

#endif
