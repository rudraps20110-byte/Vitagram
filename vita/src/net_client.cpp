#include "net_client.hpp"

#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/net/http.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/sysmodule.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace vitagram {

namespace {
constexpr const char* kDataDir = "ux0:data/VitaGram";
constexpr const char* kProxyConfigPath = "ux0:data/VitaGram/proxy.txt";
constexpr const char* kDefaultProxyUrl = "http://127.0.0.1:8787";
constexpr int kNetMemorySize = 1 * 1024 * 1024;
constexpr int kHttpPoolSize = 1 * 1024 * 1024;
constexpr size_t kMaxResponseBytes = 8 * 1024 * 1024;

bool execute_http_request(int template_id,
                          const std::string& url,
                          int method,
                          const std::string* body_data,
                          std::vector<uint8_t>& out_body,
                          int& out_status,
                          std::string& out_error) {
    out_body.clear();
    out_status = 0;
    out_error.clear();

    if (template_id < 0) {
        out_error = "HTTP client not initialized";
        return false;
    }

    int conn_id = sceHttpCreateConnectionWithURL(template_id, url.c_str(), 0);
    if (conn_id < 0) {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "Connect handle error (0x%08X)", static_cast<unsigned>(conn_id));
        out_error = buf;
        return false;
    }

    unsigned long long content_len = body_data ? static_cast<unsigned long long>(body_data->size()) : 0ULL;
    int req_id = sceHttpCreateRequestWithURL(conn_id, method, url.c_str(), content_len);
    if (req_id < 0) {
        sceHttpDeleteConnection(conn_id);
        char buf[96];
        std::snprintf(buf, sizeof(buf), "Request handle error (0x%08X)", static_cast<unsigned>(req_id));
        out_error = buf;
        return false;
    }

    if (body_data) {
        sceHttpAddRequestHeader(req_id, "Content-Type", "application/json", SCE_HTTP_HEADER_OVERWRITE);
    }

    const void* payload_ptr = body_data && !body_data->empty() ? body_data->data() : nullptr;
    unsigned int payload_len = body_data ? static_cast<unsigned int>(body_data->size()) : 0U;
    int ret = sceHttpSendRequest(req_id, payload_ptr, payload_len);
    if (ret < 0) {
        sceHttpDeleteRequest(req_id);
        sceHttpDeleteConnection(conn_id);
        char buf[128];
        std::snprintf(buf, sizeof(buf), "Proxy unreachable (0x%08X). Is server running?", static_cast<unsigned>(ret));
        out_error = buf;
        return false;
    }

    ret = sceHttpGetStatusCode(req_id, &out_status);
    if (ret < 0) {
        sceHttpDeleteRequest(req_id);
        sceHttpDeleteConnection(conn_id);
        out_error = "Failed to read HTTP status code";
        return false;
    }

    uint8_t chunk[16 * 1024];
    while (true) {
        int read_bytes = sceHttpReadData(req_id, chunk, sizeof(chunk));
        if (read_bytes < 0) {
            sceHttpDeleteRequest(req_id);
            sceHttpDeleteConnection(conn_id);
            out_error = "Error reading HTTP response body";
            return false;
        }
        if (read_bytes == 0) break;
        if (out_body.size() + static_cast<size_t>(read_bytes) > kMaxResponseBytes) {
            sceHttpDeleteRequest(req_id);
            sceHttpDeleteConnection(conn_id);
            out_error = "Response exceeded 8MB limit";
            return false;
        }
        out_body.insert(out_body.end(), chunk, chunk + read_bytes);
    }

    sceHttpDeleteRequest(req_id);
    sceHttpDeleteConnection(conn_id);
    return true;
}
} // namespace

NetClient::~NetClient() {
    term();
}

bool NetClient::init() {
    if (ready_) return true;

    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    sceSysmoduleLoadModule(SCE_SYSMODULE_HTTP);

    net_memory_ = std::malloc(kNetMemorySize);
    if (!net_memory_) return false;

    SceNetInitParam net_param;
    std::memset(&net_param, 0, sizeof(net_param));
    net_param.memory = net_memory_;
    net_param.size = kNetMemorySize;
    net_param.flags = 0;

    int net_ret = sceNetInit(&net_param);
    if (net_ret < 0) {
        std::free(net_memory_);
        net_memory_ = nullptr;
        // On Vita3K or if net is already initialized, continue cautiously
    }

    sceNetCtlInit();
    sceHttpInit(kHttpPoolSize);

    template_id_ = sceHttpCreateTemplate("VitaGram/1.0", SCE_HTTP_VERSION_1_1, SCE_HTTP_PROXY_AUTO);
    if (template_id_ < 0) {
        return false;
    }

    sceHttpSetConnectTimeOut(template_id_, 6 * 1000 * 1000);
    sceHttpSetSendTimeOut(template_id_, 6 * 1000 * 1000);
    sceHttpSetRecvTimeOut(template_id_, 10 * 1000 * 1000);
    sceHttpSetResolveTimeOut(template_id_, 5 * 1000 * 1000);

    ready_ = true;
    return true;
}

void NetClient::term() {
    if (template_id_ >= 0) {
        sceHttpDeleteTemplate(template_id_);
        template_id_ = -1;
    }
    if (ready_) {
        sceHttpTerm();
        sceNetCtlTerm();
        sceNetTerm();
        ready_ = false;
    }
    if (net_memory_) {
        std::free(net_memory_);
        net_memory_ = nullptr;
    }
    sceSysmoduleUnloadModule(SCE_SYSMODULE_HTTP);
    sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
}

bool NetClient::http_get(const std::string& url,
                         std::vector<uint8_t>& out_body,
                         int& out_status,
                         std::string& out_error) {
    if (!ready_) {
        out_body.clear();
        out_status = 0;
        out_error = "HTTP client not initialized";
        return false;
    }
    return execute_http_request(template_id_, url, SCE_HTTP_METHOD_GET, nullptr, out_body, out_status, out_error);
}

bool NetClient::http_post_json(const std::string& url,
                               const std::string& json_body,
                               std::vector<uint8_t>& out_body,
                               int& out_status,
                               std::string& out_error) {
    if (!ready_) {
        out_body.clear();
        out_status = 0;
        out_error = "HTTP client not initialized";
        return false;
    }
    return execute_http_request(template_id_, url, SCE_HTTP_METHOD_POST, &json_body, out_body, out_status, out_error);
}

std::string NetClient::url_encode(const std::string& value) {
    static const char* kHex = "0123456789ABCDEF";
    std::string out;
    out.reserve(value.size() * 3);
    for (unsigned char c : value) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(kHex[(c >> 4) & 0x0F]);
            out.push_back(kHex[c & 0x0F]);
        }
    }
    return out;
}

std::string NetClient::resolve_url(const std::string& base_url, const std::string& path_or_url) {
    if (path_or_url.empty()) return "";
    if (path_or_url.rfind("http://", 0) == 0 || path_or_url.rfind("https://", 0) == 0) {
        return path_or_url;
    }
    std::string base = base_url;
    while (!base.empty() && base.back() == '/') base.pop_back();
    if (path_or_url.front() == '/') return base + path_or_url;
    return base + "/" + path_or_url;
}

std::string NetClient::load_proxy_url() {
    SceUID fd = sceIoOpen(kProxyConfigPath, SCE_O_RDONLY, 0);
    if (fd < 0) return kDefaultProxyUrl;

    char buf[256];
    int n = sceIoRead(fd, buf, sizeof(buf) - 1);
    sceIoClose(fd);
    if (n <= 0) return kDefaultProxyUrl;
    buf[n] = '\0';

    std::string url(buf);
    while (!url.empty() && (url.back() == '\n' || url.back() == '\r' || url.back() == ' ')) {
        url.pop_back();
    }
    return url.empty() ? std::string(kDefaultProxyUrl) : url;
}

void NetClient::save_proxy_url(const std::string& url) {
    sceIoMkdir("ux0:data", 0777);
    sceIoMkdir(kDataDir, 0777);
    SceUID fd = sceIoOpen(kProxyConfigPath, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    if (fd >= 0) {
        sceIoWrite(fd, url.c_str(), static_cast<int>(url.size()));
        sceIoClose(fd);
    }
}

} // namespace vitagram

