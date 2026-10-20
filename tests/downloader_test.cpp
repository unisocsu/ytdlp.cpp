#include "ytdlp/downloader.hpp"

#include <cassert>
#include <iostream>

int main() {
    ytdlp::downloader::DownloadEngine engine;

    ytdlp::downloader::DownloadOptions options;
    auto result = engine.download("https://example.com/video-page", options);
    assert(!result.success);
    assert(result.error == "Output path must not be empty");

    options.output_path = "unused-test-output.mp4";
    result = engine.download("file:///tmp/watch?id=123", options);
    assert(!result.success);
    assert(result.error == "No native extractor supports this URL");

    result = engine.download("file:///tmp/video.mp4", options);
    assert(!result.success);
    assert(result.error == "No native extractor supports this URL");

    std::cout << "download engine tests passed\n";
    return 0;
}
