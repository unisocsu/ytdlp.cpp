#include "ytdlp/extractor.hpp"

#include "ytdlp/utils.hpp"
#include "ytdlp/html.hpp"
#include "ytdlp/network.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <regex>
#include <set>
#include <utility>

namespace ytdlp {
namespace extractor {
namespace {

const std::set<std::string>& direct_media_extensions() {
    static const std::set<std::string> extensions = {
        "3gp", "aac", "aiff", "alac", "avi", "flac", "flv", "m4a",
        "m4v", "mka", "mkv", "mov", "mp3", "mp4", "mpeg", "mpg",
        "oga", "ogg", "ogv", "opus", "ts", "wav", "webm", "wma", "wmv"
    };
    return extensions;
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool is_http_url(const std::string& url) {
    return url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0;
}

std::string remove_extension(const std::string& filename) {
    const auto dot = filename.rfind('.');
    if (dot == std::string::npos || dot == 0) return filename;
    return filename.substr(0, dot);
}


std::string json_unescape(std::string value) {
    std::string out;
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] != '\\' || i + 1 >= value.size()) { out.push_back(value[i]); continue; }
        const char c = value[++i];
        if (c == 'n') out.push_back('\n');
        else if (c == 'r') out.push_back('\r');
        else if (c == 't') out.push_back('\t');
        else if (c == '"' || c == '\\' || c == '/') out.push_back(c);
        else if (c == 'u' && i + 4 < value.size()) {
            // YouTube commonly escapes URL separators as \u0026.
            unsigned int code = 0;
            bool valid = true;
            for (int j = 0; j < 4; ++j) {
                const char h = value[i + 1 + j];
                code <<= 4;
                if (h >= '0' && h <= '9') code += static_cast<unsigned int>(h - '0');
                else if (h >= 'a' && h <= 'f') code += static_cast<unsigned int>(h - 'a' + 10);
                else if (h >= 'A' && h <= 'F') code += static_cast<unsigned int>(h - 'A' + 10);
                else { valid = false; break; }
            }
            if (valid) {
                if (code < 0x80) out.push_back(static_cast<char>(code));
                else if (code < 0x800) {
                    out.push_back(static_cast<char>(0xC0 | (code >> 6)));
                    out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                } else {
                    out.push_back(static_cast<char>(0xE0 | (code >> 12)));
                    out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                    out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                }
                i += 4;
            } else out += "\\u";
        } else out.push_back(c);
    }
    return out;
}

std::string json_string_field(const std::string& json, const std::string& key) {
    const std::string marker = "\"" + key + "\"";
    std::size_t pos = json.find(marker);
    while (pos != std::string::npos) {
        pos += marker.size();
        while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
        if (pos >= json.size() || json[pos++] != ':') {
            pos = json.find(marker, pos);
            continue;
        }
        while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
        if (pos >= json.size() || json[pos++] != '"') {
            pos = json.find(marker, pos);
            continue;
        }
        const std::size_t begin = pos;
        bool escaped = false;
        for (; pos < json.size(); ++pos) {
            if (escaped) escaped = false;
            else if (json[pos] == '\\') escaped = true;
            else if (json[pos] == '"') return json_unescape(json.substr(begin, pos - begin));
        }
        return "";
    }
    return "";
}

std::string youtube_video_id(const std::string& url) {
    std::smatch match;
    static const std::regex patterns[] = {
        std::regex(R"([?&]v=([A-Za-z0-9_-]{11}))"),
        std::regex(R"(youtu\.be/([A-Za-z0-9_-]{11}))"),
        std::regex(R"(/(?:shorts|embed|live)/([A-Za-z0-9_-]{11}))")
    };
    for (const auto& pattern : patterns)
        if (std::regex_search(url, match, pattern)) return match[1].str();
    return "";
}

std::string youtube_api_key(const std::string& html) {
    static const std::regex key_pattern(R"KEY("INNERTUBE_API_KEY"\s*:\s*"([^"]+)")KEY");
    std::smatch match;
    return std::regex_search(html, match, key_pattern) ? match[1].str() : "";
}


std::string url_decode(const std::string& s) {
    std::string o;
    auto hx=[](char c)->int { if(c>='0'&&c<='9') return c-'0'; if(c>='a'&&c<='f') return c-'a'+10; if(c>='A'&&c<='F') return c-'A'+10; return -1; };
    for (std::size_t i=0;i<s.size();++i) { if(s[i]=='+') o.push_back(' '); else if(s[i]=='%'&&i+2<s.size()&&hx(s[i+1])>=0&&hx(s[i+2])>=0) { o.push_back(static_cast<char>((hx(s[i+1])<<4)|hx(s[i+2]))); i+=2; } else o.push_back(s[i]); } return o;
}
std::string url_encode_component(const std::string& s) {
    static const char h[]="0123456789ABCDEF"; std::string o;
    for(unsigned char c:s) { if(std::isalnum(c)||c=='-'||c=='_'||c=='.'||c=='~') o.push_back(static_cast<char>(c)); else { o.push_back('%'); o.push_back(h[c>>4]); o.push_back(h[c&15]); } } return o;
}
std::string query_value(const std::string& q,const std::string& key) {
    std::size_t p=0; while(p<=q.size()) { auto e=q.find('&',p); auto part=q.substr(p,e==std::string::npos?e:e-p); auto x=part.find('='); if(url_decode(part.substr(0,x))==key) return x==std::string::npos?"":url_decode(part.substr(x+1)); if(e==std::string::npos) break; p=e+1; } return "";
}
std::size_t matching_brace(const std::string& s,std::size_t p) {
    int d=0; char q=0; bool esc=false;
    for(std::size_t i=p;i<s.size();++i) {
        char c=s[i];
        if(q) { if(esc) esc=false; else if(c=='\\') esc=true; else if(c==q) q=0; continue; }
        if(c=='"') q=c;
        else if(c=='{') ++d;
        else if(c=='}'&&--d==0) return i;
    }
    return std::string::npos;
}
std::string youtube_player_js_url(const std::string& html) {
    std::smatch m; const std::regex ps[]={std::regex(R"("jsUrl"\s*:\s*"([^"]+)")"),std::regex(R"("PLAYER_JS_URL"\s*:\s*"([^"]+)")"),std::regex(R"("js"\s*:\s*"([^"]*base\.js[^"]*)")")};
    for(const auto& p:ps) if(std::regex_search(html,m,p)) { auto v=json_unescape(m[1].str()); if(v.rfind("//",0)==0)return "https:"+v; if(v.rfind("/",0)==0)return "https://www.youtube.com"+v; if(is_http_url(v))return v; } return "";
}
bool youtube_decipher_signature(const std::string& js,const std::string& sig,std::string& out) {
    std::smatch m; std::string fn;
    const std::regex calls[]={std::regex(R"(\.sig\|\|([$\w]+)\()"),std::regex(R"(signature",\s*([$\w]+)\()"),std::regex(R"(set\("signature",\s*([$\w]+)\()")};
    for(const auto& p:calls) if(std::regex_search(js,m,p)){fn=m[1].str();break;} if(fn.empty()) return false;
    std::string body;
    const std::regex defs[]={std::regex("(?:function\\s+"+fn+R"(\s*\(\s*([\w$]+)\s*\)\s*\{)")),std::regex("(?:var\\s+)?"+fn+R"(\s*=\s*function\s*\(\s*([\w$]+)\s*\)\s*\{)")};
    for(const auto& p:defs) if(std::regex_search(js,m,p)){auto op=static_cast<std::size_t>(m.position()+m.length()-1); auto cl=matching_brace(js,op); if(cl!=std::string::npos) body=js.substr(op+1,cl-op-1); break;} if(body.empty()) return false;
    enum K{REV,DROP,SWAP}; struct Op{K k;std::size_t n;}; std::map<std::string,Op> helpers;
    std::smatch hm; std::string obj; if(std::regex_search(body,hm,std::regex(R"(([$\w]+)\.([$ \w]+)\()"))) obj=hm[1].str();
    if(!obj.empty()) { std::smatch om; if(std::regex_search(js,om,std::regex("(?:var\\s+)?"+obj+R"(\s*=\s*\{)"))) { auto op=static_cast<std::size_t>(om.position()+om.length()-1), cl=matching_brace(js,op); if(cl!=std::string::npos) { auto src=js.substr(op+1,cl-op-1); std::regex mp(R"(([$\w]+)\s*:\s*function\s*\([^)]*\)\s*\{)");
        for(auto it=std::sregex_iterator(src.begin(),src.end(),mp);it!=std::sregex_iterator();++it){auto x=*it;auto a=static_cast<std::size_t>(x.position()+x.length()-1),b=matching_brace(src,a);if(b==std::string::npos)continue;auto mb=src.substr(a+1,b-a-1);if(mb.find(".reverse(")!=std::string::npos)helpers[x[1].str()]={REV,0};else if(mb.find(".splice(0,")!=std::string::npos||mb.find(".slice(")!=std::string::npos){std::smatch n;std::size_t v=0;if(std::regex_search(mb,n,std::regex(R"((?:splice|slice)\(0,(\d+)\))")))v=std::stoul(n[1].str());helpers[x[1].str()]={DROP,v};}else if(mb.find("[0]")!=std::string::npos&&mb.find("%")!=std::string::npos)helpers[x[1].str()]={SWAP,0};}
    }}}
    std::vector<Op> ops; std::regex cr(R"((?:[$\w]+\.)?([$\w]+)\(\s*[$\w]+(?:\s*,\s*(\d+))?\s*\))");
    for(auto it=std::sregex_iterator(body.begin(),body.end(),cr);it!=std::sregex_iterator();++it){auto x=*it;auto h=helpers.find(x[1].str());if(h==helpers.end())continue;auto op=h->second;if(x[2].matched)op.n=std::stoul(x[2].str());ops.push_back(op);}
    if(ops.empty()) return false; out=sig;
    for(auto op:ops){if(op.k==REV)std::reverse(out.begin(),out.end());else if(op.k==DROP)out.erase(0,std::min(op.n,out.size()));else if(!out.empty())std::swap(out[0],out[op.n%out.size()]);} return true;
}

std::vector<Format> youtube_direct_formats(const std::string& json, const std::string& player_js = "") {
    std::vector<Format> formats;
    // Walk JSON objects while respecting quoted strings; format objects are small and
    // contain both mimeType and url. This deliberately avoids a heavyweight JSON dependency.
    for (std::size_t start = 0; start < json.size(); ++start) {
        if (json[start] != '{') continue;
        bool quoted = false, escaped = false;
        int depth = 0;
        std::size_t end = start;
        for (; end < json.size(); ++end) {
            const char c = json[end];
            if (quoted) {
                if (escaped) escaped = false;
                else if (c == '\\') escaped = true;
                else if (c == '"') quoted = false;
                continue;
            }
            if (c == '"') quoted = true;
            else if (c == '{') ++depth;
            else if (c == '}' && --depth == 0) { ++end; break; }
        }
        if (end > json.size() || depth != 0) continue;
        const std::string object = json.substr(start, end - start);
        std::string url = json_string_field(object, "url");
        const std::string mime = json_string_field(object, "mimeType");
        if (mime.empty()) continue;
        if (url.empty()) {
            std::string cipher = json_string_field(object, "signatureCipher");
            if (cipher.empty()) cipher = json_string_field(object, "cipher");
            if (!cipher.empty()) {
                const std::string base = query_value(cipher, "url");
                std::string sig = query_value(cipher, "s");
                std::string key = query_value(cipher, "sp");
                if (key.empty()) key = "signature";
                std::string decoded;
                if (!base.empty() && !sig.empty() && youtube_decipher_signature(player_js, sig, decoded))
                    url = base + (base.find('?') == std::string::npos ? "?" : "&") + key + "=" + url_encode_component(decoded);
                else if (!base.empty() && sig.empty()) url = base;
            }
        }
        if (url.empty() || !is_http_url(url)) continue;
        Format format;
        format.url = url;
        format.protocol = "https";
        format.ext = mime.find("webm") != std::string::npos ? "webm" : "mp4";
        format.format_id = json_string_field(object, "itag");
        if (format.format_id.empty()) format.format_id = "youtube-direct";
        std::smatch height;
        if (std::regex_search(object, height, std::regex(R"("height"\s*:\s*(\d+))")))
            format.height = std::stoll(height[1].str());
        std::smatch bitrate;
        if (std::regex_search(object, bitrate, std::regex(R"("bitrate"\s*:\s*(\d+))")))
            format.tbr = std::stod(bitrate[1].str()) / 1000.0;
        formats.push_back(std::move(format));
    }
    return formats;
}

std::string media_id(const std::string& url) {
    std::string id = ytdlp::utils::url_basename(url);
    if (id.empty()) return url;
    return remove_extension(id);
}

}  // namespace

const char* DirectMediaExtractor::name() const {
    return "DirectMedia";
}

bool DirectMediaExtractor::suitable(const std::string& url) const {
    if (!is_http_url(url)) return false;
    const std::string ext = ytdlp::utils::determine_ext(url, "");
    return direct_media_extensions().find(lower(ext)) != direct_media_extensions().end();
}

ExtractResult DirectMediaExtractor::extract(const std::string& url) const {
    if (!suitable(url)) return {std::nullopt, "URL is not a supported direct media file"};
    const std::string ext = lower(ytdlp::utils::determine_ext(url, ""));
    MediaInfo info;
    info.id = media_id(url);
    info.title = info.id.empty() ? "video" : info.id;
    info.original_url = url;
    info.webpage_url = url;
    info.extractor_key = name();
    info.ext = ext;
    Format format;
    format.format_id = "direct";
    format.url = url;
    format.ext = ext;
    format.protocol = url.rfind("https://", 0) == 0 ? "https" : "http";
    info.formats.push_back(std::move(format));
    return {std::move(info), ""};
}


const char* YouTubeExtractor::name() const { return "YouTube"; }

bool YouTubeExtractor::suitable(const std::string& url) const {
    return url.find("youtube.com/") != std::string::npos ||
           url.find("youtube-nocookie.com/") != std::string::npos ||
           url.find("youtu.be/") != std::string::npos;
}

ExtractResult YouTubeExtractor::extract(const std::string& url) const {
    const std::string video_id = youtube_video_id(url);
    if (video_id.empty())
        return {std::nullopt, "Could not identify a YouTube video ID from this URL"};

    ytdlp::network::HttpRequest page_request;
    page_request.url = "https://www.youtube.com/watch?v=" + video_id;
    page_request.timeout_seconds = 30;
    page_request.headers.emplace_back("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/131.0 Safari/537.36");
    page_request.headers.emplace_back("Accept-Language", "en-US,en;q=0.8");
    const auto page = ytdlp::network::perform(page_request);
    if (!page.ok())
        return {std::nullopt, "Could not fetch YouTube watch page: " +
            (page.error.empty() ? "HTTP " + std::to_string(page.status) : page.error)};
    const std::string player_js_url = youtube_player_js_url(page.body);
    const std::string api_key = youtube_api_key(page.body);
    if (api_key.empty())
        return {std::nullopt, "YouTube did not expose an Innertube API key; page layout or access restrictions may have changed"};

    // YouTube's player API can expose different format sets for different clients.
    // Try the ordinary web client first, then Android's public player client as a
    // fallback. This is not signature deciphering; encrypted/ciphered URLs are still
    // rejected explicitly below.
    std::string player_body;
    std::string player_error;
    std::string player_js;
    if (!player_js_url.empty()) {
        ytdlp::network::HttpRequest js_request;
        js_request.url = player_js_url;
        js_request.timeout_seconds = 30;
        js_request.headers.emplace_back("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/131.0 Safari/537.36");
        const auto js_response = ytdlp::network::perform(js_request);
        if (js_response.ok()) player_js = js_response.body;
    }
    std::vector<Format> formats;
    const struct PlayerClient {
        const char* name;
        const char* version;
        const char* user_agent;
        const char* context_client_extra;
        const char* context_extra;
    } clients[] = {
        {"WEB", "2.20250101.00.00",
         "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 Chrome/131.0 Safari/537.36", "", ""},
        {"ANDROID", "19.09.37",
         "com.google.android.youtube/19.09.37 (Linux; U; Android 11) gzip", "", ""},
        // yt-dlp's current extractor uses VISIONOS for some videos that do not
        // expose usable formats to the usual WEB/ANDROID clients.
        {"VISIONOS", "1.02",
         "Mozilla/5.0 (Macintosh; Intel Mac OS X 15_7_3) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/26.0 Safari/605.1.15",
         ",\"deviceMake\":\"Apple\",\"deviceModel\":\"RealityDevice17,1\",\"osName\":\"visionOS\",\"osVersion\":\"26.5.23O471\"", ""},
        {"TVHTML5_SIMPLY_EMBEDDED_PLAYER", "2.0",
         "Mozilla/5.0 (SMART-TV; Linux; Tizen 6.0) AppleWebKit/537.36 (KHTML, like Gecko) 85.0.4183.93/6.0 TV Safari/537.36",
         "", ",\"thirdParty\":{\"embedUrl\":\"https://www.youtube.com/\"}"}
    };
    for (const auto& client : clients) {
        ytdlp::network::HttpRequest player_request;
        player_request.url = "https://www.youtube.com/youtubei/v1/player?key=" + api_key;
        player_request.method = "POST";
        player_request.timeout_seconds = 30;
        player_request.headers.emplace_back("Content-Type", "application/json");
        player_request.headers.emplace_back("Origin", "https://www.youtube.com");
        player_request.headers.emplace_back("User-Agent", client.user_agent);
        player_request.body = "{\"videoId\":\"" + video_id +
            "\",\"context\":{\"client\":{\"clientName\":\"" + client.name +
            "\",\"clientVersion\":\"" + client.version +
            "\",\"hl\":\"en\",\"gl\":\"US\"" + client.context_client_extra +
            "}" + client.context_extra + "}}";
        const auto player = ytdlp::network::perform(player_request);
        if (!player.ok()) {
            player_error = player.error.empty()
                ? "HTTP " + std::to_string(player.status) : player.error;
            continue;
        }
        player_body = player.body;
        formats = youtube_direct_formats(player_body, player_js);
        if (!formats.empty()) break;
    }
    if (formats.empty()) {
        if (player_body.find("signatureCipher") != std::string::npos ||
            player_body.find("\"cipher\"") != std::string::npos) {
            return {std::nullopt, "YouTube returned ciphered media URLs, but the native signature parser did not recognize this player script. It currently supports common helper-based reverse/drop/swap transforms only."};
        }
        if (player_body.empty() && !player_error.empty())
            return {std::nullopt, "YouTube player API request failed: " + player_error};
        const std::string playability = json_string_field(player_body, "status");
        const std::string reason = json_string_field(player_body, "reason");
        if (player_body.find("hlsManifestUrl") != std::string::npos) {
            return {std::nullopt, "YouTube returned an HLS manifest for the available player client. HLS playlist parsing and segmented downloading are not implemented yet."};
        }
        if (player_body.find("adaptiveFormats") != std::string::npos ||
            player_body.find("formats") != std::string::npos) {
            return {std::nullopt, "YouTube returned format metadata, but no directly usable media URLs. Signature deciphering, JavaScript challenge support, and separate DASH track merging are not implemented yet."};
        }
        std::string detail = "YouTube returned no direct media URLs after trying WEB, ANDROID, VISIONOS, and TV embedded player clients. Signature deciphering, JavaScript challenge support, and separate DASH track merging are not implemented yet.";
        if (!playability.empty()) detail += " Player status: " + playability + ".";
        if (!reason.empty()) detail += " Reason: " + reason + ".";
        return {std::nullopt, detail};
    }
    const std::string title = json_string_field(player_body, "title");
    // Prefer a combined audio/video format where available; otherwise use the
    // highest-resolution direct format. Separate DASH tracks are not merged yet.
    std::stable_sort(formats.begin(), formats.end(), [](const Format& a, const Format& b) {
        return a.height.value_or(0) > b.height.value_or(0);
    });
    MediaInfo info;
    info.id = video_id;
    info.title = title.empty() ? video_id : title;
    info.original_url = url;
    info.webpage_url = "https://www.youtube.com/watch?v=" + video_id;
    info.extractor_key = name();
    info.ext = formats.front().ext;
    info.formats = std::move(formats);
    return {std::move(info), ""};
}

const char* GenericHtmlExtractor::name() const { return "GenericHTML"; }

bool GenericHtmlExtractor::suitable(const std::string& url) const {
    return is_http_url(url) && !DirectMediaExtractor().suitable(url);
}

ExtractResult GenericHtmlExtractor::extract(const std::string& url) const {
    if (!suitable(url)) return {std::nullopt, "URL is not a webpage URL"};
    ytdlp::network::HttpRequest request;
    request.url = url;
    request.method = "GET";
    request.timeout_seconds = 30;
    request.headers.emplace_back("User-Agent", "ytdlp.cpp/0.1 native media extractor");
    request.headers.emplace_back("Accept", "text/html,application/xhtml+xml,video/*;q=0.9,*/*;q=0.8");
    const auto response = ytdlp::network::perform(request);
    if (!response.ok()) {
        if (!response.error.empty()) return {std::nullopt, "Could not fetch webpage: " + response.error};
        return {std::nullopt, "Could not fetch webpage (HTTP " + std::to_string(response.status) + ")"};
    }
    const auto metadata = ytdlp::html::parse_metadata(response.body);
    std::string selected_url;
    std::string selected_ext;
    for (const auto& candidate : metadata.media_urls) {
        const auto joined = ytdlp::utils::url_join(std::optional<std::string>(url),
                                                   std::optional<std::string>(candidate));
        if (!joined || !is_http_url(*joined)) continue;
        const std::string ext = lower(ytdlp::utils::determine_ext(*joined, ""));
        if (direct_media_extensions().find(ext) == direct_media_extensions().end()) continue;
        selected_url = *joined;
        selected_ext = ext;
        break;
    }
    if (selected_url.empty()) return {std::nullopt,
        "Fetched the webpage, but found no direct media URL in metadata or HTML5 video/source tags. JavaScript and site-specific extractors are not supported yet."};
    MediaInfo info;
    info.id = media_id(selected_url);
    info.title = metadata.title.empty() ? info.id : metadata.title;
    if (info.title.empty()) info.title = "video";
    info.original_url = url;
    info.webpage_url = metadata.canonical_url.empty() ? url : metadata.canonical_url;
    info.extractor_key = name();
    info.ext = selected_ext;
    Format format;
    format.format_id = "html-direct";
    format.url = selected_url;
    format.ext = selected_ext;
    format.protocol = selected_url.rfind("https://", 0) == 0 ? "https" : "http";
    info.formats.push_back(std::move(format));
    return {std::move(info), ""};
}

ExtractorRegistry::ExtractorRegistry() {
    add(std::unique_ptr<InfoExtractor>(new DirectMediaExtractor()));
    add(std::unique_ptr<InfoExtractor>(new YouTubeExtractor()));
    add(std::unique_ptr<InfoExtractor>(new GenericHtmlExtractor()));
}

bool ExtractorRegistry::add(std::unique_ptr<InfoExtractor> extractor) {
    if (!extractor) return false;
    extractors_.push_back(std::move(extractor));
    return true;
}

const InfoExtractor* ExtractorRegistry::find(const std::string& url) const {
    for (const auto& extractor : extractors_)
        if (extractor && extractor->suitable(url)) return extractor.get();
    return nullptr;
}

ExtractResult ExtractorRegistry::extract(const std::string& url) const {
    const InfoExtractor* extractor = find(url);
    if (!extractor) return {std::nullopt, "No native extractor supports this URL"};
    return extractor->extract(url);
}

std::size_t ExtractorRegistry::size() const {
    return extractors_.size();
}

}  // namespace extractor
}  // namespace ytdlp
