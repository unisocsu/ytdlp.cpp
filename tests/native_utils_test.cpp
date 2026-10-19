#include "ytdlp/utils.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

using namespace ytdlp::utils;

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

    assert(std::abs(*parse_duration(std::string("01:02:03")) - 3723.0) < 0.001);
    assert(std::abs(*parse_duration(std::string("1:02")) - 62.0) < 0.001);
    assert(std::abs(*parse_duration(std::string("1h 2m 3.5s")) - 3723.5) < 0.001);
    assert(std::abs(*parse_duration(std::string("P1DT2H")) - 93600.0) < 0.001);
    assert(!parse_duration(std::string("not a duration")));
    assert(!parse_duration(std::nullopt));

    assert(url_join(std::string("https://example.com/a/b"), std::string("../video?id=1")) ==
           std::optional<std::string>("https://example.com/video?id=1"));
    assert(url_join(std::string("https://example.com/a/b"), std::string("/watch")) ==
           std::optional<std::string>("https://example.com/watch"));
    assert(update_url_query("https://example.com/watch?a=1&b=old#top", {{"b", "new value"}, {"c", "3"}}) ==
           "https://example.com/watch?a=1&b=new+value&c=3#top");
    std::cout << "native utility tests passed\n";
    return 0;
}
