#pragma once

#include "api_client.hpp"
#include "models.hpp"
#include "osk.hpp"
#include "texture_cache.hpp"

#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <string>
#include <vector>
#include <vita2d.h>

namespace vitagram {

enum class AppTab {
    Feed = 0,
    Search = 1,
    Profile = 2,
    Settings = 3,
};

enum class OskTarget {
    None,
    SearchQuery,
    ProxyUrl,
};

class AppUI {
public:
    AppUI(ApiClient& api, TextureCache& textures, vita2d_pgf* font);

    void init();
    bool update(uint32_t pressed_buttons, const SceTouchData& touch, bool touch_just_pressed);
    void draw();

private:
    ApiClient& api_;
    TextureCache& textures_;
    vita2d_pgf* font_ = nullptr;
    OnScreenKeyboard osk_;
    OskTarget osk_target_ = OskTarget::None;

    std::string proxy_url_;
    HealthStatus health_;
    bool health_checked_ = false;
    std::string status_message_;
    std::string error_message_;
    std::string loading_overlay_;

    AppTab active_tab_ = AppTab::Feed;
    AppTab previous_tab_ = AppTab::Feed;

    // Feed state
    std::vector<Post> feed_posts_;
    std::string feed_next_max_id_;
    bool feed_has_more_ = false;
    int feed_index_ = 0;
    bool feed_loaded_once_ = false;

    // Lightbox modal (for Feed or Profile post preview)
    bool lightbox_open_ = false;
    Post lightbox_post_;

    // Search state
    std::string search_query_;
    std::vector<UserSummary> search_results_;
    int search_index_ = 0; // -1 = search bar focused, >=0 = user row
    bool search_loaded_once_ = false;

    // Profile state
    UserProfile profile_;
    bool profile_loaded_ = false;
    int profile_post_index_ = 0;

    // Settings state
    int settings_index_ = 0;

    void show_loading_overlay(const std::string& label);
    void check_health_now();
    void load_feed(bool reset);
    void run_search(const std::string& query);
    void open_profile(const std::string& username);
    vita2d_texture* get_or_load_texture(const std::string& relative_or_full_url);
    void preload_pending_textures();

    void handle_osk_done(OskResult res);
    void draw_header() const;
    void draw_tab_bar() const;
    void draw_feed();
    void draw_search();
    void draw_profile();
    void draw_settings() const;
    void draw_lightbox();

    void draw_fitted_texture(vita2d_texture* tex, float box_x, float box_y, float box_w, float box_h) const;
    void draw_wrapped_text(int x, int y, int max_w, int line_h, int max_lines, unsigned int color, float scale, const std::string& text) const;
};

} // namespace vitagram
