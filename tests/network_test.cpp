#include "ytdlp/network.hpp"

#include <cassert>
#include <iostream>

int main() {
    ytdlp::network::CookieJar jar;
    jar.store_from_set_cookie("https://example.com/watch/page", "sid=abc; Path=/watch; Secure; HttpOnly");
    assert(jar.header_for_url("https://example.com/watch/video") == "sid=abc");
    assert(jar.header_for_url("http://example.com/watch/video").empty());
    assert(jar.header_for_url("https://example.com/other").empty());
    jar.store_from_set_cookie("https://example.com/watch/page", "sid=; Path=/watch; Max-Age=0");
    assert(jar.header_for_url("https://example.com/watch/video").empty());
    jar.store_from_set_cookie("https://sub.example.com/", "shared=yes; Domain=example.com; Path=/");
    assert(jar.header_for_url("https://example.com/") == "shared=yes");
    assert(jar.header_for_url("https://sub.example.com/") == "shared=yes");
    jar.clear();
    assert(jar.header_for_url("https://example.com/").empty());
    ytdlp::network::HttpResponse response;
    response.headers.emplace_back("Content-Type", "video/mp4");
    assert(response.header("content-type") == "video/mp4");
    assert(!response.ok());
    response.status = 200;
    assert(response.ok());
    std::cout << "network utility tests passed\n";
}
