#include "ytdlp/utils.hpp"

#include <cassert>
#include <iostream>

using ytdlp::utils::determine_ext;
using ytdlp::utils::int_or_none;
using ytdlp::utils::str_or_none;
using ytdlp::utils::url_basename;
using ytdlp::utils::urlencode_postdata;

int main() {
    assert(determine_ext(std::string("https://example.com/video.mp4?token=x")) == "mp4");
    assert(determine_ext(std::string("https://example.com/video/"), "bin") == "bin");
    assert(determine_ext(std::nullopt) == "unknown_video");
    assert(url_basename("https://example.com/a/b/file%20name.mp4?download=1") == "file%20name.mp4");
    assert(url_basename("https://example.com/a/b/") == "b");
    assert(int_or_none(std::string("123")) == 123);
    assert(int_or_none(std::string("-5"), 2, 1) == -3);
    assert(!int_or_none(std::string("12x")).has_value());
    assert(!int_or_none(std::nullopt).has_value());
    assert(str_or_none(std::string("value")) == std::optional<std::string>("value"));
    assert(!str_or_none(std::nullopt).has_value());
    assert(urlencode_postdata({{"name", "a b"}, {"next", "/x?y=1"}}) ==
           "name=a+b&next=%2Fx%3Fy%3D1");
    std::cout << "native utility tests passed\n";
    return 0;
}
