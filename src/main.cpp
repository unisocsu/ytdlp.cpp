#include "ytdlp/downloader.hpp"
#include "ytdlp/ytdlp.h"

#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {
void print_usage() {
    std::cerr
        << "Usage: ytdlp <URL> -o <output-file> [--timeout seconds] [--ca-bundle path]\n"
        << "       ytdlp --version\n\n"
        << "Supports direct media URLs, generic HTML pages, and initial YouTube extraction.\n"
        << "YouTube signature deciphering and separate DASH track merging are not implemented.\n";
}

bool parse_positive_long(const std::string& value, long& output) {
    if (value.empty()) return false;
    char* end = nullptr;
    errno = 0;
    const long parsed = std::strtol(value.c_str(), &end, 10);
    if (errno != 0 || end == value.c_str() || *end != '\0' || parsed <= 0) return false;
    output = parsed;
    return true;
}
} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && (std::string(argv[1]) == "--version" ||
                      std::string(argv[1]) == "-V")) {
        std::cout << "ytdlp.cpp " << ytdlp_version() << "\n";
        return 0;
    }

    if (argc < 4) {
        print_usage();
        return 2;
    }

    const std::string url = argv[1];
    ytdlp::downloader::DownloadOptions options;
    bool has_output = false;

    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-o" || arg == "--output") {
            if (i + 1 >= argc || std::string(argv[i + 1]).empty()) {
                std::cerr << "Error: " << arg << " requires a file path.\n";
                return 2;
            }
            options.output_path = argv[++i];
            has_output = true;
        } else if (arg == "--timeout") {
            if (i + 1 >= argc || !parse_positive_long(argv[i + 1], options.timeout_seconds)) {
                std::cerr << "Error: --timeout requires a positive number of seconds.\n";
                return 2;
            }
            ++i;
        } else if (arg == "--ca-bundle") {
            if (i + 1 >= argc || std::string(argv[i + 1]).empty()) {
                std::cerr << "Error: --ca-bundle requires a file path.\n";
                return 2;
            }
            options.ca_bundle_path = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            print_usage();
            return 0;
        } else {
            std::cerr << "Error: unknown argument: " << arg << "\n";
            print_usage();
            return 2;
        }
    }

    if (!has_output) {
        std::cerr << "Error: output path is required; use -o <output-file>.\n";
        print_usage();
        return 2;
    }

    ytdlp::downloader::DownloadEngine engine;
    const ytdlp::downloader::DownloadResult result = engine.download(url, options);
    if (!result.success) {
        std::cerr << "Download failed: " << result.error;
        if (result.http_status > 0) std::cerr << " (HTTP " << result.http_status << ")";
        std::cerr << "\n";
        return 1;
    }

    std::cout << "Downloaded to: " << result.output_path << "\n";
    return 0;
}
