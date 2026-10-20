#include "ytdlp/html.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <regex>
#include <string>
#include <utility>

namespace ytdlp {
namespace html {
namespace {

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string decode_entities(std::string value) {
    const std::pair<const char*, const char*> entities[] = {
        {"&amp;", "&"}, {"&quot;", "\""}, {"&#39;", "'"},
        {"&apos;", "'"}, {"&lt;", "<"}, {"&gt;", ">"}
    };
    for (const auto& entity : entities) {
        std::size_t pos = 0;
        while ((pos = value.find(entity.first, pos)) != std::string::npos) {
            value.replace(pos, std::string(entity.first).size(), entity.second);
            pos += std::string(entity.second).size();
        }
    }
    return trim(value);
}

std::map<std::string, std::string> parse_attributes(const std::string& tag) {
    static const std::regex attr(
        R"ATTR(([a-zA-Z_:][-a-zA-Z0-9_:.]*)\s*=\s*(?:"([^"]*)"|'([^']*)'|([^\s>]+)))ATTR",
        std::regex::icase);
    std::map<std::string, std::string> result;
    for (std::sregex_iterator it(tag.begin(), tag.end(), attr), end; it != end; ++it) {
        const auto& match = *it;
        std::string value = match[2].matched ? match[2].str() :
            (match[3].matched ? match[3].str() : match[4].str());
        result[lower(match[1].str())] = decode_entities(value);
    }
    return result;
}

void add_unique(std::vector<std::string>& values, const std::string& value) {
    if (value.empty()) return;
    if (std::find(values.begin(), values.end(), value) == values.end())
        values.push_back(value);
}

}  // namespace

Metadata parse_metadata(const std::string& document) {
    Metadata result;
    static const std::regex title_tag(R"(<title\b[^>]*>([\s\S]*?)</title\s*>)", std::regex::icase);
    std::smatch title_match;
    if (std::regex_search(document, title_match, title_tag))
        result.title = decode_entities(std::regex_replace(title_match[1].str(), std::regex(R"(<[^>]*>)"), ""));

    static const std::regex tag_regex(R"(<(meta|link|video|source)\b[^>]*>)", std::regex::icase);
    for (std::sregex_iterator it(document.begin(), document.end(), tag_regex), end; it != end; ++it) {
        const std::string tag = it->str();
        const std::string tag_name = lower((*it)[1].str());
        const auto attrs = parse_attributes(tag);
        const auto get = [&](const char* key) -> std::string {
            const auto found = attrs.find(key);
            return found == attrs.end() ? "" : found->second;
        };
        if (tag_name == "meta") {
            const std::string key = lower(get("property").empty() ? get("name") : get("property"));
            const std::string content = get("content");
            if ((key == "og:title" || key == "twitter:title") && result.title.empty())
                result.title = content;
            else if ((key == "og:description" || key == "description" || key == "twitter:description") && result.description.empty())
                result.description = content;
            else if (key == "og:video" || key == "og:video:url" ||
                     key == "og:video:secure_url" || key == "twitter:player:stream")
                add_unique(result.media_urls, content);
        } else if (tag_name == "link" && lower(get("rel")).find("canonical") != std::string::npos) {
            if (result.canonical_url.empty()) result.canonical_url = get("href");
        } else if (tag_name == "video" || tag_name == "source") {
            add_unique(result.media_urls, get("src"));
        }
    }
    return result;
}

}  // namespace html
}  // namespace ytdlp
