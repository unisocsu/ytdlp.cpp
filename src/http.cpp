#include "ytdlp/http.h"

#include <curl/curl.h>

#include <cstdio>
#include <string>

namespace {
thread_local std::string last_error;

struct CurlGlobal {
    CurlGlobal() : result(curl_global_init(CURL_GLOBAL_DEFAULT)) {}
    ~CurlGlobal() {
        if (result == CURLE_OK) curl_global_cleanup();
    }
    CURLcode result;
};

CurlGlobal& curl_global_state() {
    static CurlGlobal state;
    return state;
}

size_t write_file(void* data, size_t size, size_t count, void* user_data) {
    FILE* file = static_cast<FILE*>(user_data);
    return std::fwrite(data, size, count, file);
}

bool allowed_url(const char* url) {
    if (!url) return false;
    const std::string value(url);
    return value.rfind("http://", 0) == 0 ||
           value.rfind("https://", 0) == 0;
}
}

extern "C" int ytdlp_http_download(const char* url,
                                    const char* output_path,
                                    const char* ca_bundle_path,
                                    long timeout_seconds,
                                    long* http_status) {
    last_error.clear();
    if (http_status) *http_status = 0;

    if (!allowed_url(url) || !output_path || output_path[0] == '\0') {
        last_error = "URL must begin with http:// or https:// and output path must be non-empty";
        return 1;
    }

    CurlGlobal& global = curl_global_state();
    if (global.result != CURLE_OK) {
        last_error = std::string("curl_global_init failed: ") +
                     curl_easy_strerror(global.result);
        return 2;
    }

    FILE* file = std::fopen(output_path, "wb");
    if (!file) {
        last_error = std::string("Could not open output file: ") + output_path;
        return 3;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        std::fclose(file);
        last_error = "curl_easy_init failed";
        return 4;
    }

    char error_buffer[CURL_ERROR_SIZE] = {0};
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_buffer);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "ytdlp.cpp/0.1.0");
    if (timeout_seconds > 0) {
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_seconds);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, timeout_seconds < 30 ? timeout_seconds : 30L);
    }
    if (ca_bundle_path && ca_bundle_path[0] != '\0') {
        curl_easy_setopt(curl, CURLOPT_CAINFO, ca_bundle_path);
    }

    const CURLcode result = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    if (http_status) *http_status = status;

    const int close_result = std::fclose(file);
    if (result != CURLE_OK) {
        last_error = error_buffer[0] ? error_buffer : curl_easy_strerror(result);
        std::remove(output_path);
        curl_easy_cleanup(curl);
        return 5;
    }
    if (close_result != 0) {
        last_error = "Failed to close output file";
        std::remove(output_path);
        curl_easy_cleanup(curl);
        return 6;
    }
    if (status < 200 || status >= 300) {
        last_error = "HTTP request returned status " + std::to_string(status);
        std::remove(output_path);
        curl_easy_cleanup(curl);
        return 7;
    }

    curl_easy_cleanup(curl);
    return 0;
}

extern "C" const char* ytdlp_http_last_error(void) {
    return last_error.c_str();
}
