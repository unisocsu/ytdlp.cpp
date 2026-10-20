#include "ytdlp/extractor.hpp"

#include <cassert>
#include <iostream>
#include <memory>
#include <string>

using ytdlp::extractor::DirectMediaExtractor;
using ytdlp::extractor::ExtractorRegistry;

int main() {
    DirectMediaExtractor direct;
    assert(direct.suitable("https://cdn.example/video.mp4?token=abc"));
    assert(direct.suitable("http://cdn.example/clip.WEBM"));
    assert(!direct.suitable("https://example.com/watch?v=123"));
    assert(!direct.suitable("file:///tmp/video.mp4"));

    const auto result = direct.extract("https://cdn.example/media/clip.MP4?token=abc");
    assert(result);
    assert(result.info->id == "clip");
    assert(result.info->title == "clip");
    assert(result.info->ext == "mp4");
    assert(result.info->extractor_key == "DirectMedia");
    assert(result.info->formats.size() == 1);
    assert(result.info->formats[0].format_id == "direct");
    assert(result.info->formats[0].url == "https://cdn.example/media/clip.MP4?token=abc");
    assert(result.info->formats[0].protocol == "https");
    assert(!direct.extract("https://example.com/watch?v=123"));

    ExtractorRegistry registry;
    assert(registry.size() == 2);
    assert(registry.find("https://cdn.example/video.mp4") != nullptr);
    assert(registry.find("https://example.com/watch?v=123") != nullptr);
    const auto found = registry.extract("http://cdn.example/video.mp4");
    assert(found && found.info->formats.size() == 1);
    const auto missing = registry.extract("file:///tmp/watch?id=123");
    assert(!missing);
    assert(missing.error == "No native extractor supports this URL");

    std::cout << "native extractor tests passed\n";
    return 0;
}
