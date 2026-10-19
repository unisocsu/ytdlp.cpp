#include "ytdlp/utils.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <regex>
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
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
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
    if (remainder != 0 && ((remainder < 0) != (denominator < 0))) --quotient;
    return quotient;
}

bool is_unreserved(unsigned char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
}

std::string form_encode(const std::string& value) {
    static const char hex[] = "0123456789ABCDEF";
    std::string encoded;
    for (unsigned char c : value) {
        if (is_unreserved(c)) encoded.push_back(static_cast<char>(c));
        else if (c == ' ') encoded.push_back('+');
        else {
            encoded.push_back('%');
            encoded.push_back(hex[(c >> 4) & 0x0F]);
            encoded.push_back(hex[c & 0x0F]);
        }
    }
    return encoded;
}

int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string form_decode(const std::string& value) {
    std::string decoded;
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '+' ) decoded.push_back(' ');
        else if (value[i] == '%' && i + 2 < value.size() &&
                 hex_value(value[i + 1]) >= 0 && hex_value(value[i + 2]) >= 0) {
            decoded.push_back(static_cast<char>((hex_value(value[i + 1]) << 4) | hex_value(value[i + 2])));
            i += 2;
        } else decoded.push_back(value[i]);
    }
    return decoded;
}

std::string remove_dot_segments(const std::string& path) {
    const bool leading = !path.empty() && path.front() == '/';
    const bool trailing = !path.empty() && path.back() == '/';
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= path.size()) {
        const auto end = path.find('/', start);
        const std::string part = path.substr(start, end == std::string::npos ? end : end - start);
        if (part == "..") {
            if (!parts.empty()) parts.pop_back();
        } else if (!part.empty() && part != ".") parts.push_back(part);
        if (end == std::string::npos) break;
        start = end + 1;
    }
    std::string result = leading ? "/" : "";
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i) result += "/";
        result += parts[i];
    }
    if (trailing && !result.empty() && result.back() != '/') result += "/";
    if (result.empty() && leading) result = "/";
    return result;
}

}  // namespace

std::string determine_ext(const std::optional<std::string>& url,
                          const std::string& default_ext) {
    if (!url) return default_ext;
    const std::size_t end = url->find_first_of("?#");
    const std::string path = url->substr(0, end);
    const std::size_t slash = path.rfind('/');
    const std::size_t dot = path.rfind('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash + 1))
        return default_ext;
    std::string ext = path.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return is_ascii_alnum(ext) ? ext : default_ext;
}

std::string url_basename(const std::string& url) {
    std::string path = url_path(url);
    while (!path.empty() && path.back() == '/') path.pop_back();
    while (!path.empty() && path.front() == '/') path.erase(path.begin());
    const std::size_t slash = path.rfind('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::optional<std::int64_t> int_or_none(
    const std::optional<std::string>& value,
    std::int64_t scale,
    std::int64_t invscale,
    std::optional<int> base) {
    if (!value || scale == 0) return std::nullopt;
    try {
        std::size_t consumed = 0;
        const int parse_base = base.value_or(10);
        if (parse_base != 0 && (parse_base < 2 || parse_base > 36)) return std::nullopt;
        const long long parsed = std::stoll(*value, &consumed, parse_base);
        while (consumed < value->size() &&
               std::isspace(static_cast<unsigned char>((*value)[consumed]))) ++consumed;
        if (consumed != value->size()) return std::nullopt;
        const auto p = static_cast<std::int64_t>(parsed);
        if (invscale != 0) {
            if (invscale > 0 && (p > std::numeric_limits<std::int64_t>::max() / invscale ||
                                 p < std::numeric_limits<std::int64_t>::min() / invscale))
                return std::nullopt;
            if (invscale < 0 && (p == std::numeric_limits<std::int64_t>::min() ||
                                 -p > std::numeric_limits<std::int64_t>::max() / -invscale))
                return std::nullopt;
        }
        const std::int64_t product = p * invscale;
        if (scale == -1 && product == std::numeric_limits<std::int64_t>::min()) return std::nullopt;
        return floor_div(product, scale);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::optional<std::string> str_or_none(const std::optional<std::string>& value) {
    return value;
}

std::string urlencode_postdata(
    const std::vector<std::pair<std::string, std::string>>& fields) {
    std::string result;
    bool first = true;
    for (const auto& field : fields) {
        if (!first) result.push_back('&');
        first = false;
        result += form_encode(field.first);
        result.push_back('=');
        result += form_encode(field.second);
    }
    return result;
}

std::optional<double> parse_duration(const std::optional<std::string>& value) {
    if (!value) return std::nullopt;
    const std::string s = std::regex_replace(*value, std::regex("^\s+|\s+$"), "");
    if (s.empty()) return std::nullopt;
    std::smatch m;

    // Clock forms: SS, MM:SS, HH:MM:SS, DD:HH:MM:SS; fraction is seconds.
    const std::regex clock(R"(^([0-9]+(?::[0-9]+){0,3})(?:\.([0-9]+))?Z?$)", std::regex::icase);
    if (std::regex_match(s, m, clock)) {
        try {
            std::vector<double> fields;
            const std::string clock_part = m[1].str();
            std::size_t begin = 0;
            while (begin <= clock_part.size()) {
                const auto colon = clock_part.find(':', begin);
                fields.push_back(std::stod(clock_part.substr(begin,
                    colon == std::string::npos ? colon : colon - begin)));
                if (colon == std::string::npos) break;
                begin = colon + 1;
            }
            double total = 0.0;
            if (fields.size() == 1) total = fields[0];
            else if (fields.size() == 2) total = fields[0] * 60.0 + fields[1];
            else if (fields.size() == 3) total = fields[0] * 3600.0 + fields[1] * 60.0 + fields[2];
            else total = fields[0] * 86400.0 + fields[1] * 3600.0 + fields[2] * 60.0 + fields[3];
            if (m[2].matched) total += std::stod("0." + m[2].str());
            return std::isfinite(total) ? std::optional<double>(total) : std::nullopt;
        } catch (...) { return std::nullopt; }
    }

    // Common unit strings and ISO-8601 day/time subset, e.g. "1h 2m 3.5s" or "P1DT2H".
    std::string unit_text = s;
    if (!unit_text.empty() && (unit_text.front() == 'P' || unit_text.front() == 'p'))
        unit_text.erase(unit_text.begin());
    double total = 0.0;
    bool found_unit = false;
    std::size_t pos = 0;
    try {
        while (pos < unit_text.size()) {
            while (pos < unit_text.size() &&
                   (std::isspace(static_cast<unsigned char>(unit_text[pos])) ||
                    unit_text[pos] == ',' || unit_text[pos] == 'T' || unit_text[pos] == 't')) ++pos;
            if (pos == unit_text.size()) break;
            const char* begin = unit_text.c_str() + pos;
            char* number_end = nullptr;
            const double amount = std::strtod(begin, &number_end);
            if (number_end == begin || !std::isfinite(amount)) return std::nullopt;
            pos = static_cast<std::size_t>(number_end - unit_text.c_str());
            while (pos < unit_text.size() && std::isspace(static_cast<unsigned char>(unit_text[pos]))) ++pos;
            const std::size_t unit_start = pos;
            while (pos < unit_text.size() &&
                   std::isalpha(static_cast<unsigned char>(unit_text[pos]))) ++pos;
            std::string unit = unit_text.substr(unit_start, pos - unit_start);
            std::transform(unit.begin(), unit.end(), unit.begin(),
                [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            if (!unit.empty() && unit.back() == 'z') unit.pop_back();
            if (unit == "d" || unit == "day" || unit == "days") total += amount * 86400.0;
            else if (unit == "h" || unit == "hr" || unit == "hrs" ||
                     unit == "hour" || unit == "hours") total += amount * 3600.0;
            else if (unit == "m" || unit == "min" || unit == "mins" ||
                     unit == "minute" || unit == "minutes") total += amount * 60.0;
            else if (unit == "s" || unit == "sec" || unit == "secs" ||
                     unit == "second" || unit == "seconds") total += amount;
            else return std::nullopt;
            found_unit = true;
            if (pos < unit_text.size() && !std::isspace(static_cast<unsigned char>(unit_text[pos])) &&
                unit_text[pos] != ',' && unit_text[pos] != 'T' && unit_text[pos] != 't')
                return std::nullopt;
        }
        if (found_unit && std::isfinite(total)) return total;
    } catch (...) {
        return std::nullopt;
    }
    return std::nullopt;
}

std::optional<std::string> url_join(
    const std::optional<std::string>& base,
    const std::optional<std::string>& relative) {
    if (!relative) return std::nullopt;
    if (!base || relative->empty()) return relative;
    const std::string& b = *base;
    const std::string& r = *relative;
    if (r.find("://") != std::string::npos || r.rfind("//", 0) == 0) {
        if (r.rfind("//", 0) == 0) {
            const auto colon = b.find(':');
            return (colon == std::string::npos ? std::string("https:") : b.substr(0, colon + 1)) + r;
        }
        return r;
    }
    const auto scheme = b.find("://");
    if (scheme == std::string::npos) {
        if (r.front() == '/') return r;
        const auto slash = b.rfind('/');
        return slash == std::string::npos ? r : b.substr(0, slash + 1) + r;
    }
    const auto authority_end = b.find('/', scheme + 3);
    const std::string origin = authority_end == std::string::npos ? b : b.substr(0, authority_end);
    if (r.front() == '/') return origin + remove_dot_segments(r);
    const auto suffix = b.find_first_of("?#");
    const std::string clean_base = b.substr(0, suffix);
    const auto last_slash = clean_base.rfind('/');
    const std::string directory = last_slash == std::string::npos || last_slash < scheme + 3
        ? origin + "/" : clean_base.substr(0, last_slash + 1);
    const auto relative_suffix = r.find_first_of("?#");
    const std::string relative_path = r.substr(0, relative_suffix);
    const std::string tail = relative_suffix == std::string::npos ? "" : r.substr(relative_suffix);
    const std::string combined_path = directory.substr(origin.size()) + relative_path;
    return origin + remove_dot_segments(combined_path) + tail;
}

std::string update_url_query(
    const std::string& url,
    const std::vector<std::pair<std::string, std::string>>& updates) {
    const auto hash = url.find('#');
    const std::string fragment = hash == std::string::npos ? "" : url.substr(hash);
    const std::string no_fragment = url.substr(0, hash);
    const auto q = no_fragment.find('?');
    const std::string path = q == std::string::npos ? no_fragment : no_fragment.substr(0, q);
    std::vector<std::pair<std::string, std::string>> fields;
    if (q != std::string::npos && q + 1 < no_fragment.size()) {
        const std::string query = no_fragment.substr(q + 1);
        std::size_t start = 0;
        while (start <= query.size()) {
            const auto amp = query.find('&', start);
            const std::string item = query.substr(start, amp == std::string::npos ? amp : amp - start);
            const auto eq = item.find('=');
            if (!item.empty()) fields.emplace_back(form_decode(item.substr(0, eq)),
                eq == std::string::npos ? "" : form_decode(item.substr(eq + 1)));
            if (amp == std::string::npos) break;
            start = amp + 1;
        }
    }
    for (const auto& update : updates) {
        fields.erase(std::remove_if(fields.begin(), fields.end(), [&](const auto& field) {
            return field.first == update.first;
        }), fields.end());
        fields.push_back(update);
    }
    return path + (fields.empty() ? "" : "?" + urlencode_postdata(fields)) + fragment;
}

}  // namespace utils
}  // namespace ytdlp
