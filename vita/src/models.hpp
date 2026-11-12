#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace vitagram {

struct Post {
    std::string id;
    std::string username;
    std::string full_name;
    std::string avatar_url;
    std::string image_url;
    std::string caption;
    int64_t like_count = 0;
    int64_t comment_count = 0;
    std::string timestamp_text;
    bool is_video = false;
};

struct FeedPage {
    std::vector<Post> posts;
    std::string next_max_id;
    bool has_more = false;
};

struct UserSummary {
    std::string username;
    std::string full_name;
    std::string avatar_url;
    bool is_verified = false;
    int64_t follower_count = 0;
};

struct UserProfile {
    std::string username;
    std::string full_name;
    std::string bio;
    std::string avatar_url;
    bool is_verified = false;
    bool is_private = false;
    int64_t post_count = 0;
    int64_t follower_count = 0;
    int64_t following_count = 0;
    std::vector<Post> posts;
};

struct HealthStatus {
    bool ok = false;
    std::string mode;
    std::string username;
    bool sharp_enabled = false;
};

} // namespace vitagram

