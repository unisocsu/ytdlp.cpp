#include "ytdlp/network.hpp"

#include <curl/curl.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace ytdlp {
namespace network {
namespace {

struct ParsedUrl {
    std::string scheme;
    std::string host;
    std::string path;
};

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

bool parse_url(const std::string& url, ParsedUrl& out) {
    const auto scheme_end = url.find("://");
    if (scheme_end == std::string::npos) return false;
    out.scheme = lower(url.substr(0, scheme_end));
    if (out.scheme != "http" && out.scheme != "https") return false;
    const auto host_start = scheme_end + 3;
    const auto path_start = url.find_first_of("/?#", host_start);
    const std::string authority = url.substr(host_start,
        path_start == std::string::npos ? path_start : path_start - host_start);
    if (authority.empty() || authority.find('@') != std::string::npos) return false;
    out.host = lower(authority);
    if (path_start == std::string::npos || url[path_start] == '?' || url[path_start] == '#')
        out.path = "/";
    else out.path = url.substr(path_start, url.find_first_of("?#", path_start) - path_start);
    return true;
}

bool domain_matches(const std::string& host, const std::string& domain) {
    if (host == domain) return true;
    return host.size() > domain.size() &&
        host.compare(host.size() - domain.size(), domain.size(), domain) == 0 &&
        host[host.size() - domain.size() - 1] == '.';
}

size_t body_callback(char* data, size_t size, size_t count, void* opaque) {
    auto* body = static_cast<std::string*>(opaque);
    const size_t bytes = size * count;
    body->append(data, bytes);
    return bytes;
}

size_t header_callback(char* data, size_t size, size_t count, void* opaque) {
    auto* headers = static_cast<std::vector<std::pair<std::string, std::string>>*>(opaque);
    const size_t bytes = size * count;
    const std::string line(data, bytes);
    const auto colon = line.find(':');
    if (colon != std::string::npos) {
        const std::string name = trim(line.substr(0, colon));
        const std::string value = trim(line.substr(colon + 1));
        if (!name.empty()) headers->emplace_back(name, value);
    }
    return bytes;
}

}  // namespace

std::string HttpResponse::header(const std::string& name) const {
    const std::string wanted = lower(name);
    for (auto it = headers.rbegin(); it != headers.rend(); ++it)
        if (lower(it->first) == wanted) return it->second;
    return "";
}

void CookieJar::store_from_set_cookie(const std::string& request_url,
                                      const std::string& set_cookie) {
    ParsedUrl parsed;
    if (!parse_url(request_url, parsed)) return;
    std::istringstream stream(set_cookie);
    std::string part;
    if (!std::getline(stream, part, ';')) return;
    part = trim(part);
    const auto eq = part.find('=');
    if (eq == std::string::npos || eq == 0) return;
    Cookie cookie;
    cookie.name = trim(part.substr(0, eq));
    cookie.value = trim(part.substr(eq + 1));
    cookie.domain = parsed.host;
    cookie.path = "/";
    while (std::getline(stream, part, ';')) {
        part = trim(part);
        const auto attr_eq = part.find('=');
        const std::string attr = lower(trim(part.substr(0, attr_eq)));
        const std::string value = attr_eq == std::string::npos ? "" : trim(part.substr(attr_eq + 1));
        if (attr == "domain" && !value.empty()) {
            cookie.domain = lower(value);
            if (!cookie.domain.empty() && cookie.domain.front() == '.') cookie.domain.erase(cookie.domain.begin());
            cookie.host_only = false;
        } else if (attr == "path" && !value.empty() && value.front() == '/') {
            cookie.path = value;
        } else if (attr == "secure") {
            cookie.secure = true;
        } else if (attr == "max-age") {
            char* end = nullptr;
            const long age = std::strtol(value.c_str(), &end, 10);
            if (end != value.c_str() && age <= 0) {
                cookies_.erase(std::remove_if(cookies_.begin(), cookies_.end(), [&](const Cookie& old) {
                    return old.name == cookie.name && old.domain == cookie.domain && old.path == cookie.path;
                }), cookies_.end());
                return;
            }
        }
    }
    cookies_.erase(std::remove_if(cookies_.begin(), cookies_.end(), [&](const Cookie& old) {
        return old.name == cookie.name && old.domain == cookie.domain && old.path == cookie.path;
    }), cookies_.end());
    cookies_.push_back(cookie);
}

std::string CookieJar::header_for_url(const std::string& url) const {
    ParsedUrl parsed;
    if (!parse_url(url, parsed)) return "";
    std::string result;
    for (const auto& cookie : cookies_) {
        if ((cookie.host_only ? parsed.host == cookie.domain : domain_matches(parsed.host, cookie.domain)) &&
            parsed.path.compare(0, cookie.path.size(), cookie.path) == 0 &&
            (!cookie.secure || parsed.scheme == "https")) {
            if (!result.empty()) result += "; ";
            result += cookie.name + "=" + cookie.value;
        }
    }
    return result;
}

void CookieJar::clear() {
    cookies_.clear();
}

HttpResponse perform(const HttpRequest& request, CookieJar* cookies) {
    HttpResponse response;
    ParsedUrl parsed;
    if (!parse_url(request.url, parsed)) {
        response.error = "URL must be an absolute HTTP or HTTPS URL";
        return response;
    }
    static const int initialized = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (initialized != CURLE_OK) {
        response.error = "curl_global_init failed";
        return response;
    }
    CURL* curl = curl_easy_init();
    if (!curl) {
        response.error = "curl_easy_init failed";
        return response;
    }
    curl_slist* request_headers = nullptr;
    for (const auto& header : request.headers) {
        const std::string line = header.first + ": " + header.second;
        request_headers = curl_slist_append(request_headers, line.c_str());
    }
    if (cookies) {
        const std::string cookie_header = cookies->header_for_url(request.url);
        if (!cookie_header.empty()) {
            const std::string line = "Cookie: " + cookie_header;
            request_headers = curl_slist_append(request_headers, line.c_str());
        }
    }
    curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, request.method.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, body_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response.headers);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, request_headers);
    if (!request.body.empty() || request.method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.data());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(request.body.size()));
    }
    if (request.timeout_seconds > 0) {
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, request.timeout_seconds);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, request.timeout_seconds < 30 ? request.timeout_seconds : 30L);
    }
    if (!request.ca_bundle_path.empty()) curl_easy_setopt(curl, CURLOPT_CAINFO, request.ca_bundle_path.c_str());

    const CURLcode code = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status);
    if (code != CURLE_OK) response.error = curl_easy_strerror(code);
    if (cookies) {
        for (const auto& header : response.headers)
            if (lower(header.first) == "set-cookie") cookies->store_from_set_cookie(request.url, header.second);
    }
    if (request_headers) curl_slist_free_all(request_headers);
    curl_easy_cleanup(curl);
    return response;
}

}  // namespace network
}  // namespace ytdlp
