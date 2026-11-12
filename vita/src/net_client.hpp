#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace vitagram {

class NetClient {
public:
    NetClient() = default;
    ~NetClient();

    NetClient(const NetClient&) = delete;
    NetClient& operator=(const NetClient&) = delete;

    bool init();
    void term();
    bool is_ready() const { return ready_; }

    bool http_get(const std::string& url,
                  std::vector<uint8_t>& out_body,
                  int& out_status,
                  std::string& out_error);

    static std::string url_encode(const std::string& value);
    static std::string resolve_url(const std::string& base_url, const std::string& path_or_url);

    static std::string load_proxy_url();
    static void save_proxy_url(const std::string& url);

private:
    bool ready_ = false;
    void* net_memory_ = nullptr;
    int template_id_ = -1;
};

} // namespace vitagram

