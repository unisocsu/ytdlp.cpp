#include "ytdlp/utils.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <set>
#include <stdexcept>

namespace ytdlp {
namespace utils {
namespace {

const std::set<std::string>& known_extensions() {
    static const std::set<std::string> extensions = {
        "3gp", "aac", "avi", "flac", "flv", "gif", "m4a", "m4v",
        "mkv", "mov", "mp3", "mp4", "mpeg", "mpg", "oga", "ogg",
        "ogv", "opus", "wav", "webm", "wma", "wmv"
    };
    return extensions;
}

bool is_ascii_alnum(const std::string& value) {
    return !value.empty() && std::all_of(value.begin(), value.end(), [](unsigned char c) {
        return (c >= 'A' && c <= 'Z') ||
               (c >= 'a' && c <= 'z') ||
               (c >= '0' && c <= '9');
    });
}

std::string url_path(const std::string& url) {
    const std::size_t end = url.find_first_of("?#");
    const std::string without_query = url.substr(0, end);
    const std::size_t scheme = without_query.find("://");
    if (scheme != std::string::npos) {
        const std::size_t path_start = without_query.find('/', scheme + 3);
        return path_start == std::string::npos ? std::string() : without_query.substr(path_start);
    }
    if (without_query.rfind("//", 0) == 0) {
        const std::size_t path_start = without_query.find('/', 2);
        return path_start == std::string::npos ? std::string() : without_query.substr(path_start);
    }
    return without_query;
}

std::int64_t floor_div(std::int64_t numerator, std::int64_t denominator) {
    std::int64_t quotient = numerator / denominator;
    const std::int64_t remainder = numerator % denominator;
    if (remainder != 0 && ((remainder < 0) != (denominator < 0))) {
        --quotient;
    }
    return quotient;
}

bool is_unreserved(unsigned char c) {
    return (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') ||
           c == '-' || c == '_' || c == '.' || c == '~';
}

std::string form_encode(const std::string& value) {
    static const char hex[] = "0123456789ABCDEF";
    std::string encoded;
    for (unsigned char c : value) {
        if (is_unreserved(c)) {
            encoded.push_back(static_cast<char>(c));
        } else if (c == ' ') {
            encoded.push_back('+');
        } else {
            encoded.push_back('%');
            encoded.push_back(hex[(c >> 4) & 0x0F]);
            encoded.push_back(hex[c & 0x0F]);
        }
    }
    return encoded;
}

}  // namespace

std::string determine_ext(const std::optional<std::string>& url,
                          const std::string& default_ext) {
    if (!url || url->find('.') == std::string::npos) {
        return default_ext;
    }

    const std::size_t query = url->find('?');
    const std::string before_query = url->substr(0, query);
    const std::size_t dot = before_query.rfind('.');
    if (dot == std::string::npos) {
        return default_ext;
    }

    std::string guess = before_query.substr(dot + 1);
    if (is_ascii_alnum(guess)) {
        return guess;
    }

    while (!guess.empty() && guess.back() == '/') {
        guess.pop_back();
    }
    return known_extensions().count(guess) ? guess : default_ext;
}

std::string url_basename(const std::string& url) {
    std::string path = url_path(url);
    while (!path.empty() && path.back() == '/') {
        path.pop_back();
    }
    while (!path.empty() && path.front() == '/') {
        path.erase(path.begin());
    }
    const std::size_t slash = path.rfind('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::optional<std::int64_t> int_or_none(
    const std::optional<std::string>& value,
    std::int64_t scale,
    std::int64_t invscale,
    std::optional<int> base) {
    if (!value || scale == 0) {
        return std::nullopt;
    }

    try {
        std::size_t consumed = 0;
        const int parse_base = base.value_or(10);
        if (parse_base != 0 && (parse_base < 2 || parse_base > 36)) {
            return std::nullopt;
        }
        const long long parsed = std::stoll(*value, &consumed, parse_base);
        while (consumed < value->size() &&
               std::isspace(static_cast<unsigned char>((*value)[consumed]))) {
            ++consumed;
        }
        if (consumed != value->size()) {
            return std::nullopt;
        }

        if (invscale != 0 &&
            (parsed > std::numeric_limits<std::int64_t>::max() / invscale ||
             parsed < std::numeric_limits<std::int64_t>::min() / invscale)) {
            return std::nullopt;
        }
        return floor_div(static_cast<std::int64_t>(parsed) * invscale, scale);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::optional<std::string> str_or_none(
    const std::optional<std::string>& value) {
    return value;
}

std::string urlencode_postdata(
    const std::vector<std::pair<std::string, std::string>>& fields) {
    std::string result;
    bool first = true;
    for (const auto& field : fields) {
        if (!first) {
            result.push_back('&');
        }
        first = false;
        result += form_encode(field.first);
        result.push_back('=');
        result += form_encode(field.second);
    }
    return result;
}

}  // namespace utils
}  // namespace ytdlp
