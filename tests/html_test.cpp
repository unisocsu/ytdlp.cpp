#include "ytdlp/html.hpp"

#include <cassert>
#include <iostream>

int main() {
    const std::string page =
        "<html><head><title>Plain &amp; simple</title>"
        "<meta content='Open Graph title' property='og:title'>"
        "<meta name=\"description\" content=\"A &amp; B\">"
        "<link rel=\"canonical\" href=\"https://example.test/watch\">"
        "<meta property=\"og:video:url\" content=\"https://cdn.example/video.mp4\">"
        "</head><body><video><source src=\"https://cdn.example/other.webm\"></video></body></html>";
    const auto metadata = ytdlp::html::parse_metadata(page);
    assert(metadata.title == "Plain & simple");
    assert(metadata.description == "A & B");
    assert(metadata.canonical_url == "https://example.test/watch");
    assert(metadata.media_urls.size() == 2);
    assert(metadata.media_urls[0] == "https://cdn.example/video.mp4");
    assert(metadata.media_urls[1] == "https://cdn.example/other.webm");

    const auto fallback = ytdlp::html::parse_metadata(
        "<meta property='og:title' content='Only metadata title'>");
    assert(fallback.title == "Only metadata title");
    std::cout << "native HTML metadata tests passed\n";
}
