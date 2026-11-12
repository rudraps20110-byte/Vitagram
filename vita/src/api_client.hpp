#pragma once

#include "models.hpp"
#include "net_client.hpp"

#include <string>
#include <vector>
#include <vita2d.h>

namespace vitagram {

class ApiClient {
public:
    explicit ApiClient(NetClient& net) : net_(net) {}

    bool check_health(const std::string& base_url, HealthStatus& out_status, std::string& out_error);
    bool fetch_feed(const std::string& base_url,
                    const std::string& max_id,
                    int limit,
                    FeedPage& out_page,
                    std::string& out_error);
    bool search_users(const std::string& base_url,
                      const std::string& query,
                      std::vector<UserSummary>& out_users,
                      std::string& out_error);
    bool fetch_profile(const std::string& base_url,
                       const std::string& username,
                       UserProfile& out_profile,
                       std::string& out_error);

    bool fetch_inbox(const std::string& base_url,
                     std::vector<MessageThreadSummary>& out_threads,
                     std::string& out_error);
    bool fetch_thread(const std::string& base_url,
                      const std::string& thread_id,
                      MessageThread& out_thread,
                      std::string& out_error);
    bool send_message(const std::string& base_url,
                      const std::string& thread_id,
                      const std::string& text,
                      DirectMessage& out_message,
                      std::string& out_error);

    vita2d_texture* fetch_jpeg_texture(const std::string& base_url, const std::string& path_or_url);

private:
    NetClient& net_;
};

} // namespace vitagram

