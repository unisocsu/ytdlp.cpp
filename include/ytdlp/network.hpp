#ifndef YTDLP_CPP_NETWORK_HPP
#define YTDLP_CPP_NETWORK_HPP

#include <string>
#include <utility>
#include <vector>

namespace ytdlp {
namespace network {

struct HttpRequest {
    std::string url;
    std::string method = "GET";
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
    long timeout_seconds = 30;
    std::string ca_bundle_path;
};

struct HttpResponse {
    long status = 0;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
    std::string error;

    bool ok() const { return error.empty() && status >= 200 && status < 300; }
    std::string header(const std::string& name) const;
};

/* Small in-memory cookie store with host/path/secure matching. */
class CookieJar {
public:
    void store_from_set_cookie(const std::string& request_url,
                               const std::string& set_cookie);
    std::string header_for_url(const std::string& url) const;
    void clear();

private:
    struct Cookie {
        std::string name;
        std::string value;
        std::string domain;
        std::string path;
        bool host_only = true;
        bool secure = false;
    };
    std::vector<Cookie> cookies_;
};

/* Performs one HTTP(S) request. TLS certificate and hostname checks stay enabled. */
HttpResponse perform(const HttpRequest& request, CookieJar* cookies = nullptr);

}  // namespace network
}  // namespace ytdlp

#endif
