#include "api_client.hpp"
#include "json.hpp"

#include <cstdio>

namespace vitagram {

namespace {

Post parse_post(const JsonValue& v) {
    Post p;
    p.id = v.get("id").as_string();
    p.username = v.get("username").as_string();
    p.full_name = v.get("full_name").as_string();
    p.avatar_url = v.get("avatar_url").as_string();
    p.image_url = v.get("image_url").as_string();
    p.caption = v.get("caption").as_string();
    p.like_count = v.get("like_count").as_int();
    p.comment_count = v.get("comment_count").as_int();
    p.timestamp_text = v.get("timestamp_text").as_string();
    p.is_video = v.get("is_video").as_bool();
    return p;
}

bool get_json(NetClient& net, const std::string& url, JsonValue& out_json, std::string& out_error) {
    std::vector<uint8_t> body;
    int status = 0;
    if (!net.http_get(url, body, status, out_error)) {
        return false;
    }

    std::string text(body.begin(), body.end());
    std::string parse_err;
    if (!parse_json(text, out_json, parse_err)) {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "Invalid JSON (HTTP %d): %s", status, parse_err.c_str());
        out_error = buf;
        return false;
    }

    if (status < 200 || status >= 300) {
        std::string api_err = out_json.get("error").as_string();
        if (!api_err.empty()) {
            out_error = api_err;
        } else {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "Server returned HTTP %d", status);
            out_error = buf;
        }
        return false;
    }
    return true;
}

} // namespace

bool ApiClient::check_health(const std::string& base_url, HealthStatus& out_status, std::string& out_error) {
    out_status = HealthStatus{};
    JsonValue root;
    if (!get_json(net_, NetClient::resolve_url(base_url, "/api/health"), root, out_error)) {
        return false;
    }
    out_status.ok = root.get("ok").as_bool();
    out_status.mode = root.get("mode").as_string("demo");
    out_status.username = root.get("username").as_string();
    out_status.sharp_enabled = root.get("sharp_enabled").as_bool();
    return out_status.ok;
}

bool ApiClient::fetch_feed(const std::string& base_url,
                           const std::string& max_id,
                           int limit,
                           FeedPage& out_page,
                           std::string& out_error) {
    out_page = FeedPage{};
    char path[256];
    if (max_id.empty()) {
        std::snprintf(path, sizeof(path), "/api/feed?limit=%d", limit);
    } else {
        std::snprintf(path, sizeof(path), "/api/feed?limit=%d&max_id=%s", limit,
                      NetClient::url_encode(max_id).c_str());
    }

    JsonValue root;
    if (!get_json(net_, NetClient::resolve_url(base_url, path), root, out_error)) {
        return false;
    }

    for (const auto& item : root.get("posts").as_array()) {
        out_page.posts.push_back(parse_post(item));
    }
    out_page.next_max_id = root.get("next_max_id").as_string();
    out_page.has_more = root.get("has_more").as_bool();
    return true;
}

bool ApiClient::search_users(const std::string& base_url,
                             const std::string& query,
                             std::vector<UserSummary>& out_users,
                             std::string& out_error) {
    out_users.clear();
    std::string url = NetClient::resolve_url(base_url, "/api/search?q=" + NetClient::url_encode(query));
    JsonValue root;
    if (!get_json(net_, url, root, out_error)) {
        return false;
    }
    for (const auto& item : root.get("users").as_array()) {
        UserSummary u;
        u.username = item.get("username").as_string();
        u.full_name = item.get("full_name").as_string();
        u.avatar_url = item.get("avatar_url").as_string();
        u.is_verified = item.get("is_verified").as_bool();
        u.follower_count = item.get("follower_count").as_int();
        out_users.push_back(std::move(u));
    }
    return true;
}

bool ApiClient::fetch_profile(const std::string& base_url,
                              const std::string& username,
                              UserProfile& out_profile,
                              std::string& out_error) {
    out_profile = UserProfile{};
    std::string url = NetClient::resolve_url(
        base_url, "/api/profile/" + NetClient::url_encode(username));
    JsonValue root;
    if (!get_json(net_, url, root, out_error)) {
        return false;
    }
    out_profile.username = root.get("username").as_string();
    out_profile.full_name = root.get("full_name").as_string();
    out_profile.bio = root.get("bio").as_string();
    out_profile.avatar_url = root.get("avatar_url").as_string();
    out_profile.is_verified = root.get("is_verified").as_bool();
    out_profile.is_private = root.get("is_private").as_bool();
    out_profile.post_count = root.get("post_count").as_int();
    out_profile.follower_count = root.get("follower_count").as_int();
    out_profile.following_count = root.get("following_count").as_int();
    for (const auto& item : root.get("posts").as_array()) {
        out_profile.posts.push_back(parse_post(item));
    }
    return true;
}

vita2d_texture* ApiClient::fetch_jpeg_texture(const std::string& base_url, const std::string& path_or_url) {
    std::string full_url = NetClient::resolve_url(base_url, path_or_url);
    if (full_url.empty()) return nullptr;

    std::vector<uint8_t> bytes;
    int status = 0;
    std::string err;
    if (!net_.http_get(full_url, bytes, status, err) || status != 200 || bytes.size() < 4) {
        return nullptr;
    }
    return vita2d_load_JPEG_buffer(bytes.data(), static_cast<unsigned long>(bytes.size()));
}

} // namespace vitagram

