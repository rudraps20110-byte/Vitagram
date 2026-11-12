#include "ui.hpp"

#include <algorithm>
#include <cstdio>
#include <sstream>

namespace vitagram {

namespace {

constexpr int kScreenW = 960;
constexpr int kScreenH = 544;
constexpr int kHeaderH = 46;
constexpr int kFooterH = 50;

std::string format_count(int64_t n) {
    char buf[32];
    if (n >= 1000000) {
        std::snprintf(buf, sizeof(buf), "%.1fM", static_cast<double>(n) / 1000000.0);
    } else if (n >= 10000) {
        std::snprintf(buf, sizeof(buf), "%.1fK", static_cast<double>(n) / 1000.0);
    } else {
        std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(n));
    }
    return buf;
}

std::string truncate_to_width(vita2d_pgf* font, float scale, const std::string& text, int max_w) {
    if (vita2d_pgf_text_width(font, scale, text.c_str()) <= max_w) return text;
    std::string out = text;
    while (out.size() > 3) {
        out.pop_back();
        std::string cand = out + "...";
        if (vita2d_pgf_text_width(font, scale, cand.c_str()) <= max_w) return cand;
    }
    return "...";
}

} // namespace

AppUI::AppUI(ApiClient& api, TextureCache& textures, vita2d_pgf* font)
    : api_(api), textures_(textures), font_(font) {}

void AppUI::init() {
    proxy_url_ = NetClient::load_proxy_url();
    check_health_now();
    load_feed(true);
}

void AppUI::show_loading_overlay(const std::string& label) {
    loading_overlay_ = label;
    draw();
    loading_overlay_.clear();
}

void AppUI::check_health_now() {
    std::string err;
    health_checked_ = true;
    if (api_.check_health(proxy_url_, health_, err)) {
        error_message_.clear();
        status_message_ = std::string("Connected (") + health_.mode + " mode)";
    } else {
        health_.ok = false;
        error_message_ = err;
        status_message_ = "Offline";
    }
}

void AppUI::load_feed(bool reset) {
    show_loading_overlay(reset ? "Loading feed..." : "Loading more posts...");
    std::string err;
    FeedPage page;
    std::string cursor = reset ? "" : feed_next_max_id_;
    if (api_.fetch_feed(proxy_url_, cursor, 5, page, err)) {
        if (reset) {
            feed_posts_.clear();
            feed_index_ = 0;
        }
        for (auto& p : page.posts) {
            feed_posts_.push_back(std::move(p));
        }
        feed_next_max_id_ = page.next_max_id;
        feed_has_more_ = page.has_more;
        feed_loaded_once_ = true;
        error_message_.clear();
    } else {
        error_message_ = err;
    }
}

void AppUI::run_search(const std::string& query) {
    search_query_ = query;
    show_loading_overlay("Searching users...");
    std::string err;
    if (api_.search_users(proxy_url_, query, search_results_, err)) {
        search_loaded_once_ = true;
        search_index_ = search_results_.empty() ? -1 : 0;
        error_message_.clear();
    } else {
        error_message_ = err;
    }
}

void AppUI::open_profile(const std::string& username) {
    if (username.empty()) return;
    previous_tab_ = active_tab_;
    active_tab_ = AppTab::Profile;
    show_loading_overlay("Loading @" + username + "...");
    std::string err;
    UserProfile prof;
    if (api_.fetch_profile(proxy_url_, username, prof, err)) {
        profile_ = std::move(prof);
        profile_loaded_ = true;
        profile_post_index_ = 0;
        error_message_.clear();
    } else {
        error_message_ = err;
    }
}

vita2d_texture* AppUI::get_or_load_texture(const std::string& url) {
    if (url.empty()) return nullptr;
    vita2d_texture* tex = textures_.get(url);
    if (tex || textures_.has_failed(url)) return tex;
    tex = api_.fetch_jpeg_texture(proxy_url_, url);
    textures_.put(url, tex);
    return tex;
}

void AppUI::preload_pending_textures() {
    // Load at most 1 missing texture per frame so the UI stays responsive
    if (lightbox_open_) {
        if (!lightbox_post_.image_url.empty() &&
            !textures_.get(lightbox_post_.image_url) &&
            !textures_.has_failed(lightbox_post_.image_url)) {
            get_or_load_texture(lightbox_post_.image_url);
        }
        return;
    }

    if (active_tab_ == AppTab::Feed && !feed_posts_.empty()) {
        const Post& p = feed_posts_[feed_index_];
        if (!p.image_url.empty() && !textures_.get(p.image_url) && !textures_.has_failed(p.image_url)) {
            get_or_load_texture(p.image_url);
            return;
        }
        if (!p.avatar_url.empty() && !textures_.get(p.avatar_url) && !textures_.has_failed(p.avatar_url)) {
            get_or_load_texture(p.avatar_url);
            return;
        }
    } else if (active_tab_ == AppTab::Search) {
        for (const auto& u : search_results_) {
            if (!u.avatar_url.empty() && !textures_.get(u.avatar_url) && !textures_.has_failed(u.avatar_url)) {
                get_or_load_texture(u.avatar_url);
                return;
            }
        }
    } else if (active_tab_ == AppTab::Profile && profile_loaded_) {
        if (!profile_.avatar_url.empty() && !textures_.get(profile_.avatar_url) && !textures_.has_failed(profile_.avatar_url)) {
            get_or_load_texture(profile_.avatar_url);
            return;
        }
        int count = std::min(static_cast<int>(profile_.posts.size()), 8);
        for (int i = 0; i < count; ++i) {
            const auto& url = profile_.posts[i].image_url;
            if (!url.empty() && !textures_.get(url) && !textures_.has_failed(url)) {
                get_or_load_texture(url);
                return;
            }
        }
    }
}

void AppUI::handle_osk_done(OskResult res) {
    if (res == OskResult::Confirmed) {
        if (osk_target_ == OskTarget::SearchQuery) {
            run_search(osk_.text());
        } else if (osk_target_ == OskTarget::ProxyUrl) {
            std::string next = osk_.text();
            while (!next.empty() && next.back() == '/') next.pop_back();
            if (!next.empty()) {
                proxy_url_ = next;
                NetClient::save_proxy_url(proxy_url_);
                textures_.clear();
                check_health_now();
                load_feed(true);
            }
        }
    }
    osk_target_ = OskTarget::None;
}

bool AppUI::update(uint32_t pressed_buttons, const SceTouchData& touch, bool touch_just_pressed) {
    if (pressed_buttons & SCE_CTRL_SELECT) {
        return false; // Optional quick exit via SELECT if needed, START only used inside OSK
    }

    if (osk_.is_open()) {
        OskResult r = osk_.update(pressed_buttons, touch, touch_just_pressed);
        if (r != OskResult::Active) {
            handle_osk_done(r);
        }
        return true;
    }

    if (lightbox_open_) {
        if (pressed_buttons & (SCE_CTRL_CIRCLE | SCE_CTRL_TRIANGLE | SCE_CTRL_CROSS)) {
            lightbox_open_ = false;
        }
        return true;
    }

    // Touch tab bar switching
    if (touch_just_pressed && touch.reportNum > 0) {
        int tx = touch.report[0].x / 2;
        int ty = touch.report[0].y / 2;
        if (ty >= kScreenH - kFooterH) {
            int tab = tx / (kScreenW / 4);
            if (tab >= 0 && tab <= 3) {
                active_tab_ = static_cast<AppTab>(tab);
                if (active_tab_ == AppTab::Search && !search_loaded_once_) {
                    run_search("");
                } else if (active_tab_ == AppTab::Profile && !profile_loaded_) {
                    std::string target = !feed_posts_.empty() ? feed_posts_[feed_index_].username : "vita_explorer";
                    open_profile(target);
                }
            }
            return true;
        }
    }

    // L / R shoulder buttons switch tabs
    if (pressed_buttons & SCE_CTRL_LTRIGGER) {
        int t = (static_cast<int>(active_tab_) + 3) % 4;
        active_tab_ = static_cast<AppTab>(t);
        if (active_tab_ == AppTab::Search && !search_loaded_once_) run_search("");
        if (active_tab_ == AppTab::Profile && !profile_loaded_) {
            open_profile(!feed_posts_.empty() ? feed_posts_[feed_index_].username : "vita_explorer");
        }
    } else if (pressed_buttons & SCE_CTRL_RTRIGGER) {
        int t = (static_cast<int>(active_tab_) + 1) % 4;
        active_tab_ = static_cast<AppTab>(t);
        if (active_tab_ == AppTab::Search && !search_loaded_once_) run_search("");
        if (active_tab_ == AppTab::Profile && !profile_loaded_) {
            open_profile(!feed_posts_.empty() ? feed_posts_[feed_index_].username : "vita_explorer");
        }
    }

    switch (active_tab_) {
        case AppTab::Feed: {
            if (pressed_buttons & SCE_CTRL_SQUARE) {
                check_health_now();
                load_feed(true);
            } else if (!feed_posts_.empty()) {
                if (pressed_buttons & (SCE_CTRL_DOWN | SCE_CTRL_RIGHT)) {
                    if (feed_index_ + 1 < static_cast<int>(feed_posts_.size())) {
                        ++feed_index_;
                    } else if (feed_has_more_) {
                        load_feed(false);
                        if (feed_index_ + 1 < static_cast<int>(feed_posts_.size())) {
                            ++feed_index_;
                        }
                    }
                } else if (pressed_buttons & (SCE_CTRL_UP | SCE_CTRL_LEFT)) {
                    if (feed_index_ > 0) --feed_index_;
                } else if (pressed_buttons & SCE_CTRL_CROSS) {
                    open_profile(feed_posts_[feed_index_].username);
                } else if (pressed_buttons & SCE_CTRL_TRIANGLE) {
                    lightbox_post_ = feed_posts_[feed_index_];
                    lightbox_open_ = true;
                }
            }
            break;
        }

        case AppTab::Search: {
            if (!search_loaded_once_) {
                run_search("");
            }
            if (pressed_buttons & SCE_CTRL_TRIANGLE) {
                osk_target_ = OskTarget::SearchQuery;
                osk_.open("Search Instagram Users", search_query_, 48);
            } else if (pressed_buttons & SCE_CTRL_UP) {
                if (search_index_ > -1) --search_index_;
            } else if (pressed_buttons & SCE_CTRL_DOWN) {
                if (search_index_ + 1 < static_cast<int>(search_results_.size())) ++search_index_;
            } else if (pressed_buttons & SCE_CTRL_CROSS) {
                if (search_index_ < 0 || search_results_.empty()) {
                    osk_target_ = OskTarget::SearchQuery;
                    osk_.open("Search Instagram Users", search_query_, 48);
                } else {
                    open_profile(search_results_[search_index_].username);
                }
            }
            break;
        }

        case AppTab::Profile: {
            if (!profile_loaded_) {
                open_profile(!feed_posts_.empty() ? feed_posts_[feed_index_].username : "vita_explorer");
            }
            if (pressed_buttons & SCE_CTRL_CIRCLE) {
                if (previous_tab_ != AppTab::Profile) {
                    active_tab_ = previous_tab_;
                }
            } else if (pressed_buttons & SCE_CTRL_SQUARE) {
                if (!profile_.username.empty()) open_profile(profile_.username);
            } else if (!profile_.posts.empty()) {
                int max_grid = std::min(static_cast<int>(profile_.posts.size()), 8);
                if (pressed_buttons & SCE_CTRL_RIGHT) {
                    if (profile_post_index_ + 1 < max_grid) ++profile_post_index_;
                } else if (pressed_buttons & SCE_CTRL_LEFT) {
                    if (profile_post_index_ > 0) --profile_post_index_;
                } else if (pressed_buttons & SCE_CTRL_DOWN) {
                    if (profile_post_index_ + 4 < max_grid) profile_post_index_ += 4;
                } else if (pressed_buttons & SCE_CTRL_UP) {
                    if (profile_post_index_ - 4 >= 0) profile_post_index_ -= 4;
                } else if (pressed_buttons & SCE_CTRL_CROSS) {
                    lightbox_post_ = profile_.posts[profile_post_index_];
                    lightbox_open_ = true;
                }
            }
            break;
        }

        case AppTab::Settings: {
            if (pressed_buttons & SCE_CTRL_UP) {
                settings_index_ = (settings_index_ + 2) % 3;
            } else if (pressed_buttons & SCE_CTRL_DOWN) {
                settings_index_ = (settings_index_ + 1) % 3;
            } else if (pressed_buttons & SCE_CTRL_CROSS) {
                if (settings_index_ == 0) {
                    osk_target_ = OskTarget::ProxyUrl;
                    osk_.open("Companion Proxy URL", proxy_url_, 96);
                } else if (settings_index_ == 1) {
                    check_health_now();
                    if (health_.ok) load_feed(true);
                } else if (settings_index_ == 2) {
                    proxy_url_ = "http://127.0.0.1:8787";
                    NetClient::save_proxy_url(proxy_url_);
                    textures_.clear();
                    check_health_now();
                    if (health_.ok) load_feed(true);
                }
            }
            break;
        }
    }

    preload_pending_textures();
    return true;
}

void AppUI::draw_fitted_texture(vita2d_texture* tex, float box_x, float box_y, float box_w, float box_h) const {
    if (!tex) return;
    float tw = static_cast<float>(vita2d_texture_get_width(tex));
    float th = static_cast<float>(vita2d_texture_get_height(tex));
    if (tw <= 0.0f || th <= 0.0f) return;

    float sx = box_w / tw;
    float sy = box_h / th;
    float s = std::min(sx, sy);
    float dw = tw * s;
    float dh = th * s;
    float dx = box_x + (box_w - dw) * 0.5f;
    float dy = box_y + (box_h - dh) * 0.5f;
    vita2d_draw_texture_scale(tex, dx, dy, s, s);
}

void AppUI::draw_wrapped_text(int x, int y, int max_w, int line_h, int max_lines, unsigned int color, float scale, const std::string& text) const {
    if (text.empty() || max_lines <= 0) return;

    std::istringstream iss(text);
    std::string word;
    std::string line;
    int drawn = 0;

    while (iss >> word) {
        std::string cand = line.empty() ? word : (line + " " + word);
        if (vita2d_pgf_text_width(font_, scale, cand.c_str()) <= max_w) {
            line = std::move(cand);
        } else {
            if (!line.empty()) {
                if (drawn == max_lines - 1) {
                    std::string ell = truncate_to_width(font_, scale, line + " ...", max_w);
                    vita2d_pgf_draw_text(font_, x, y + drawn * line_h, color, scale, ell.c_str());
                    return;
                }
                vita2d_pgf_draw_text(font_, x, y + drawn * line_h, color, scale, line.c_str());
                ++drawn;
            }
            line = word;
        }
    }
    if (!line.empty() && drawn < max_lines) {
        std::string last = truncate_to_width(font_, scale, line, max_w);
        vita2d_pgf_draw_text(font_, x, y + drawn * line_h, color, scale, last.c_str());
    }
}

void AppUI::draw_header() const {
    vita2d_draw_rectangle(0, 0, kScreenW, kHeaderH, RGBA8(18, 20, 27, 255));
    vita2d_draw_rectangle(0, kHeaderH - 2, kScreenW, 2, RGBA8(38, 42, 54, 255));

    // Brand badge
    vita2d_draw_rectangle(16, 10, 26, 26, RGBA8(225, 48, 108, 255));
    vita2d_pgf_draw_text(font_, 23, 29, RGBA8(255, 255, 255, 255), 0.9f, "V");
    vita2d_pgf_draw_text(font_, 52, 30, RGBA8(255, 255, 255, 255), 1.1f, "VitaGram");

    // Mode pill on the right
    std::string badge = health_.ok
        ? (health_.mode == "live" ? "LIVE" : "DEMO")
        : "OFFLINE";
    unsigned int badge_col = health_.ok
        ? (health_.mode == "live" ? RGBA8(46, 204, 113, 255) : RGBA8(0, 149, 246, 255))
        : RGBA8(214, 48, 49, 255);
    vita2d_draw_rectangle(850, 10, 94, 26, badge_col);
    int bw = vita2d_pgf_text_width(font_, 0.8f, badge.c_str());
    vita2d_pgf_draw_text(font_, 850 + (94 - bw) / 2, 28, RGBA8(255, 255, 255, 255), 0.8f, badge.c_str());

    // Proxy info
    std::string info = truncate_to_width(font_, 0.8f, proxy_url_, 360);
    vita2d_pgf_draw_text(font_, 470, 29, RGBA8(140, 148, 165, 255), 0.8f, info.c_str());
}

void AppUI::draw_tab_bar() const {
    const int y = kScreenH - kFooterH;
    vita2d_draw_rectangle(0, y, kScreenW, kFooterH, RGBA8(18, 20, 27, 255));
    vita2d_draw_rectangle(0, y, kScreenW, 2, RGBA8(38, 42, 54, 255));

    const char* labels[4] = {"Feed", "Search", "Profile", "Settings"};
    const int tab_w = kScreenW / 4;
    for (int i = 0; i < 4; ++i) {
        bool active = (static_cast<int>(active_tab_) == i);
        if (active) {
            vita2d_draw_rectangle(i * tab_w, y, tab_w, 3, RGBA8(225, 48, 108, 255));
        }
        unsigned int col = active ? RGBA8(255, 255, 255, 255) : RGBA8(135, 142, 158, 255);
        int tw = vita2d_pgf_text_width(font_, 0.95f, labels[i]);
        vita2d_pgf_draw_text(font_, i * tab_w + (tab_w - tw) / 2, y + 32, col, 0.95f, labels[i]);
    }
}

void AppUI::draw_feed() {
    if (!error_message_.empty() && feed_posts_.empty()) {
        vita2d_draw_rectangle(60, 110, 840, 280, RGBA8(24, 26, 35, 255));
        vita2d_pgf_draw_text(font_, 90, 160, RGBA8(255, 107, 107, 255), 1.1f, "Could not reach VitaGram Companion Proxy");
        draw_wrapped_text(90, 205, 780, 26, 3, RGBA8(215, 220, 230, 255), 0.9f, error_message_);
        vita2d_pgf_draw_text(font_, 90, 300, RGBA8(160, 168, 185, 255), 0.88f,
                             "1. Start the Node.js proxy on your PC:  npm start  (in server/)");
        vita2d_pgf_draw_text(font_, 90, 330, RGBA8(160, 168, 185, 255), 0.88f,
                             "2. Press [] (Square) to retry, or R -> Settings to edit Proxy URL.");
        return;
    }

    if (feed_posts_.empty()) {
        vita2d_pgf_draw_text(font_, 360, 260, RGBA8(180, 185, 200, 255), 1.0f, "No posts loaded. Press [] to refresh.");
        return;
    }

    const Post& p = feed_posts_[feed_index_];

    // Left: Photo frame
    const float img_x = 24.0f;
    const float img_y = 66.0f;
    const float img_w = 420.0f;
    const float img_h = 408.0f;
    vita2d_draw_rectangle(img_x, img_y, img_w, img_h, RGBA8(16, 18, 24, 255));

    vita2d_texture* photo = textures_.get(p.image_url);
    if (photo) {
        draw_fitted_texture(photo, img_x + 4, img_y + 4, img_w - 8, img_h - 8);
    } else {
        const char* msg = textures_.has_failed(p.image_url) ? "Image unavailable" : "Loading image...";
        int mw = vita2d_pgf_text_width(font_, 0.9f, msg);
        vita2d_pgf_draw_text(font_, static_cast<int>(img_x + (img_w - mw) / 2), 270,
                             RGBA8(140, 148, 165, 255), 0.9f, msg);
    }

    if (p.is_video) {
        vita2d_draw_rectangle(img_x + 12, img_y + 12, 72, 24, RGBA8(0, 0, 0, 190));
        vita2d_pgf_draw_text(font_, static_cast<int>(img_x + 20), static_cast<int>(img_y + 30),
                             RGBA8(255, 255, 255, 255), 0.78f, "VIDEO");
    }

    // Right: Post details card
    const int card_x = 464;
    const int card_y = 66;
    const int card_w = 472;
    const int card_h = 408;
    vita2d_draw_rectangle(card_x, card_y, card_w, card_h, RGBA8(22, 24, 33, 255));

    // Author avatar + handle
    vita2d_texture* avatar = textures_.get(p.avatar_url);
    if (avatar) {
        draw_fitted_texture(avatar, static_cast<float>(card_x + 18), static_cast<float>(card_y + 18), 48.0f, 48.0f);
    } else {
        vita2d_draw_rectangle(card_x + 18, card_y + 18, 48, 48, RGBA8(52, 58, 74, 255));
    }

    std::string handle = "@" + p.username;
    vita2d_pgf_draw_text(font_, card_x + 78, card_y + 38, RGBA8(255, 255, 255, 255), 1.0f,
                         truncate_to_width(font_, 1.0f, handle, 280).c_str());
    vita2d_pgf_draw_text(font_, card_x + 78, card_y + 60, RGBA8(150, 158, 175, 255), 0.8f,
                         truncate_to_width(font_, 0.8f, p.full_name, 280).c_str());
    if (!p.timestamp_text.empty()) {
        int tw = vita2d_pgf_text_width(font_, 0.78f, p.timestamp_text.c_str());
        vita2d_pgf_draw_text(font_, card_x + card_w - 18 - tw, card_y + 38,
                             RGBA8(135, 142, 158, 255), 0.78f, p.timestamp_text.c_str());
    }

    // Divider
    vita2d_draw_rectangle(card_x + 18, card_y + 80, card_w - 36, 1, RGBA8(42, 46, 60, 255));

    // Stats row
    char stats[128];
    std::snprintf(stats, sizeof(stats), "Likes: %s    Comments: %s",
                  format_count(p.like_count).c_str(), format_count(p.comment_count).c_str());
    vita2d_pgf_draw_text(font_, card_x + 18, card_y + 110, RGBA8(240, 100, 140, 255), 0.9f, stats);

    // Caption
    draw_wrapped_text(card_x + 18, card_y + 146, card_w - 36, 26, 7,
                      RGBA8(230, 234, 242, 255), 0.88f,
                      p.caption.empty() ? "(No caption)" : p.caption);

    // Post counter + controls
    char counter[64];
    std::snprintf(counter, sizeof(counter), "Post %d / %d%s",
                  feed_index_ + 1, static_cast<int>(feed_posts_.size()),
                  feed_has_more_ ? "+" : "");
    vita2d_pgf_draw_text(font_, card_x + 18, card_y + card_h - 46, RGBA8(140, 148, 165, 255), 0.82f, counter);
    vita2d_pgf_draw_text(font_, card_x + 18, card_y + card_h - 18, RGBA8(120, 180, 255, 255), 0.78f,
                         "Up/Down: Browse   X: Profile   /\\: Zoom   []: Refresh");
}

void AppUI::draw_search() {
    // Search bar
    bool bar_focused = (search_index_ < 0);
    unsigned int bar_bg = bar_focused ? RGBA8(38, 44, 62, 255) : RGBA8(24, 26, 35, 255);
    vita2d_draw_rectangle(24, 62, 912, 46, bar_bg);
    if (bar_focused) vita2d_draw_rectangle(24, 106, 912, 2, RGBA8(225, 48, 108, 255));

    std::string prompt = search_query_.empty()
        ? "Press /\\ or X to search Instagram usernames..."
        : ("Search: " + search_query_);
    vita2d_pgf_draw_text(font_, 40, 92,
                         search_query_.empty() ? RGBA8(150, 158, 175, 255) : RGBA8(255, 255, 255, 255),
                         0.95f, truncate_to_width(font_, 0.95f, prompt, 860).c_str());

    if (!error_message_.empty() && search_results_.empty()) {
        vita2d_pgf_draw_text(font_, 40, 155, RGBA8(255, 107, 107, 255), 0.9f, error_message_.c_str());
        return;
    }

    if (search_results_.empty()) {
        vita2d_pgf_draw_text(font_, 340, 250, RGBA8(160, 168, 185, 255), 0.95f, "No matching users found.");
        return;
    }

    const int max_rows = 5;
    int start = std::max(0, search_index_ - max_rows + 1);
    if (search_index_ < 0) start = 0;

    for (int i = 0; i < max_rows && (start + i) < static_cast<int>(search_results_.size()); ++i) {
        int idx = start + i;
        const UserSummary& u = search_results_[idx];
        int ry = 122 + i * 70;
        bool selected = (idx == search_index_);
        vita2d_draw_rectangle(24, ry, 912, 62, selected ? RGBA8(40, 44, 60, 255) : RGBA8(22, 24, 33, 255));
        if (selected) vita2d_draw_rectangle(24, ry, 4, 62, RGBA8(225, 48, 108, 255));

        vita2d_texture* av = textures_.get(u.avatar_url);
        if (av) {
            draw_fitted_texture(av, 36.0f, static_cast<float>(ry + 7), 48.0f, 48.0f);
        } else {
            vita2d_draw_rectangle(36, ry + 7, 48, 48, RGBA8(52, 58, 74, 255));
        }

        std::string title = "@" + u.username + (u.is_verified ? " [V]" : "");
        vita2d_pgf_draw_text(font_, 98, ry + 28, RGBA8(255, 255, 255, 255), 0.95f, title.c_str());
        vita2d_pgf_draw_text(font_, 98, ry + 50, RGBA8(150, 158, 175, 255), 0.8f,
                             truncate_to_width(font_, 0.8f, u.full_name, 520).c_str());

        if (u.follower_count > 0) {
            std::string fc = format_count(u.follower_count) + " followers";
            int fw = vita2d_pgf_text_width(font_, 0.8f, fc.c_str());
            vita2d_pgf_draw_text(font_, 916 - fw, ry + 36, RGBA8(140, 148, 165, 255), 0.8f, fc.c_str());
        }
    }

    vita2d_pgf_draw_text(font_, 28, 480, RGBA8(120, 180, 255, 255), 0.78f,
                         "X: Open Profile   /\\: New Search   L/R: Switch Tab");
}

void AppUI::draw_profile() {
    if (!profile_loaded_) {
        vita2d_pgf_draw_text(font_, 340, 250, RGBA8(170, 175, 190, 255), 1.0f,
                             error_message_.empty() ? "No profile loaded." : error_message_.c_str());
        return;
    }

    // Profile header card
    vita2d_draw_rectangle(24, 58, 912, 134, RGBA8(22, 24, 33, 255));

    vita2d_texture* av = textures_.get(profile_.avatar_url);
    if (av) {
        draw_fitted_texture(av, 42.0f, 76.0f, 84.0f, 84.0f);
    } else {
        vita2d_draw_rectangle(42, 76, 84, 84, RGBA8(52, 58, 74, 255));
    }

    std::string handle = "@" + profile_.username + (profile_.is_verified ? " [V]" : "");
    vita2d_pgf_draw_text(font_, 144, 86, RGBA8(255, 255, 255, 255), 1.05f, handle.c_str());
    vita2d_pgf_draw_text(font_, 144, 110, RGBA8(165, 172, 188, 255), 0.85f,
                         truncate_to_width(font_, 0.85f, profile_.full_name, 380).c_str());
    draw_wrapped_text(144, 136, 460, 22, 2, RGBA8(220, 224, 232, 255), 0.82f, profile_.bio);

    // Counters on right side of header
    char counts[128];
    std::snprintf(counts, sizeof(counts), "%s posts", format_count(profile_.post_count).c_str());
    vita2d_pgf_draw_text(font_, 640, 92, RGBA8(255, 255, 255, 255), 0.9f, counts);
    std::snprintf(counts, sizeof(counts), "%s followers", format_count(profile_.follower_count).c_str());
    vita2d_pgf_draw_text(font_, 640, 122, RGBA8(255, 255, 255, 255), 0.9f, counts);
    std::snprintf(counts, sizeof(counts), "%s following", format_count(profile_.following_count).c_str());
    vita2d_pgf_draw_text(font_, 640, 152, RGBA8(255, 255, 255, 255), 0.9f, counts);

    // 4x2 post thumbnail grid
    if (profile_.posts.empty()) {
        const char* empty_msg = profile_.is_private ? "This account is private." : "No posts yet.";
        vita2d_pgf_draw_text(font_, 370, 320, RGBA8(160, 168, 185, 255), 0.95f, empty_msg);
        return;
    }

    int count = std::min(static_cast<int>(profile_.posts.size()), 8);
    const int cell_w = 216;
    const int cell_h = 132;
    const int gap = 16;
    for (int i = 0; i < count; ++i) {
        int r = i / 4;
        int c = i % 4;
        int x = 24 + c * (cell_w + gap);
        int y = 204 + r * (cell_h + 12);
        bool sel = (i == profile_post_index_);

        vita2d_draw_rectangle(x, y, cell_w, cell_h, RGBA8(16, 18, 24, 255));
        vita2d_texture* thumb = textures_.get(profile_.posts[i].image_url);
        if (thumb) {
            draw_fitted_texture(thumb, static_cast<float>(x + 3), static_cast<float>(y + 3),
                                static_cast<float>(cell_w - 6), static_cast<float>(cell_h - 6));
        } else {
            vita2d_pgf_draw_text(font_, x + 64, y + 72, RGBA8(130, 138, 155, 255), 0.8f, "Loading...");
        }
        if (sel) {
            vita2d_draw_rectangle(x, y, cell_w, 4, RGBA8(225, 48, 108, 255));
            vita2d_draw_rectangle(x, y + cell_h - 4, cell_w, 4, RGBA8(225, 48, 108, 255));
            vita2d_draw_rectangle(x, y, 4, cell_h, RGBA8(225, 48, 108, 255));
            vita2d_draw_rectangle(x + cell_w - 4, y, 4, cell_h, RGBA8(225, 48, 108, 255));
        }
    }
}

void AppUI::draw_settings() const {
    vita2d_draw_rectangle(40, 66, 880, 150, RGBA8(22, 24, 33, 255));
    vita2d_pgf_draw_text(font_, 64, 100, RGBA8(255, 255, 255, 255), 1.05f, "Companion Proxy Connection");

    std::string url_line = "Proxy URL : " + proxy_url_;
    vita2d_pgf_draw_text(font_, 64, 134, RGBA8(200, 210, 225, 255), 0.9f, url_line.c_str());

    std::string st_line = std::string("Status    : ") + (status_message_.empty() ? "Unknown" : status_message_);
    vita2d_pgf_draw_text(font_, 64, 164,
                         health_.ok ? RGBA8(85, 239, 196, 255) : RGBA8(255, 118, 117, 255),
                         0.9f, st_line.c_str());

    std::string sharp_line = std::string("WebP Transcode (sharp): ") + (health_.sharp_enabled ? "Enabled" : "Demo / JPEG passthrough");
    vita2d_pgf_draw_text(font_, 64, 194, RGBA8(160, 168, 185, 255), 0.85f, sharp_line.c_str());

    const char* items[3] = {
        "Edit Proxy URL (use http://127.0.0.1:8787 for Vita3K, or PC LAN IP for real Vita)",
        "Test Connection & Reload Feed",
        "Reset Proxy URL to Vita3K Localhost (http://127.0.0.1:8787)",
    };

    for (int i = 0; i < 3; ++i) {
        int y = 236 + i * 62;
        bool sel = (settings_index_ == i);
        vita2d_draw_rectangle(40, y, 880, 50, sel ? RGBA8(42, 46, 64, 255) : RGBA8(22, 24, 33, 255));
        if (sel) vita2d_draw_rectangle(40, y, 5, 50, RGBA8(225, 48, 108, 255));
        vita2d_pgf_draw_text(font_, 64, y + 32, RGBA8(255, 255, 255, 255), 0.9f, items[i]);
    }

    if (!error_message_.empty()) {
        vita2d_pgf_draw_text(font_, 44, 456, RGBA8(255, 118, 117, 255), 0.85f,
                             truncate_to_width(font_, 0.85f, "Last error: " + error_message_, 870).c_str());
    }
}

void AppUI::draw_lightbox() {
    vita2d_draw_rectangle(0, 0, kScreenW, kScreenH, RGBA8(8, 9, 12, 255));
    vita2d_texture* photo = textures_.get(lightbox_post_.image_url);
    if (photo) {
        draw_fitted_texture(photo, 20.0f, 16.0f, 920.0f, 460.0f);
    } else {
        vita2d_pgf_draw_text(font_, 410, 250, RGBA8(180, 185, 200, 255), 1.0f, "Loading photo...");
    }
    vita2d_draw_rectangle(0, 486, kScreenW, 58, RGBA8(16, 18, 24, 235));
    std::string cap = "@" + lightbox_post_.username + "  -  " + lightbox_post_.caption;
    vita2d_pgf_draw_text(font_, 24, 514, RGBA8(245, 245, 250, 255), 0.86f,
                         truncate_to_width(font_, 0.86f, cap, 760).c_str());
    vita2d_pgf_draw_text(font_, 810, 514, RGBA8(140, 148, 165, 255), 0.82f, "O / /\\: Close");
}

void AppUI::draw() {
    vita2d_start_drawing();
    vita2d_clear_screen();

    if (lightbox_open_) {
        draw_lightbox();
    } else {
        draw_header();
        switch (active_tab_) {
            case AppTab::Feed: draw_feed(); break;
            case AppTab::Search: draw_search(); break;
            case AppTab::Profile: draw_profile(); break;
            case AppTab::Settings: draw_settings(); break;
        }
        draw_tab_bar();
    }

    if (osk_.is_open()) {
        osk_.draw(font_);
    }

    if (!loading_overlay_.empty()) {
        vita2d_draw_rectangle(0, 0, kScreenW, kScreenH, RGBA8(0, 0, 0, 120));
        vita2d_draw_rectangle(280, 232, 400, 80, RGBA8(20, 22, 30, 245));
        vita2d_draw_rectangle(280, 232, 400, 3, RGBA8(225, 48, 108, 255));
        int tw = vita2d_pgf_text_width(font_, 1.0f, loading_overlay_.c_str());
        vita2d_pgf_draw_text(font_, 480 - tw / 2, 280, RGBA8(255, 255, 255, 255), 1.0f, loading_overlay_.c_str());
    }

    vita2d_end_drawing();
    vita2d_swap_buffers();
}

} // namespace vitagram
