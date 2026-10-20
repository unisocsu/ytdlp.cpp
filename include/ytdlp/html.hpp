#ifndef YTDLP_CPP_HTML_HPP
#define YTDLP_CPP_HTML_HPP

#include <string>
#include <vector>

namespace ytdlp {
namespace html {

/* Common metadata found in HTML pages, including Open Graph and Twitter cards. */
struct Metadata {
    std::string title;
    std::string description;
    std::string canonical_url;
    std::vector<std::string> media_urls;
};

/* Extracts common page metadata without executing JavaScript. */
Metadata parse_metadata(const std::string& document);

}  // namespace html
}  // namespace ytdlp

#endif
