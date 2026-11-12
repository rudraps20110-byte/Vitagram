#include "osk.hpp"

#include <algorithm>
#include <cstring>

namespace vitagram {

namespace {

constexpr int kRows = 5;
constexpr int kCols = 11;

const char* kKeysLower[kRows][kCols] = {
    {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-"},
    {"q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "_"},
    {"a", "s", "d", "f", "g", "h", "j", "k", "l", ".", "/"},
    {"z", "x", "c", "v", "b", "n", "m", ":", "@", "DEL", "CLR"},
    {"SHIFT", "SPACE", "SPACE", "SPACE", "http://", ":8787", "OK", "OK", "CANCEL", "CANCEL", ""},
};

const char* kKeysUpper[kRows][kCols] = {
    {"!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "+"},
    {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "_"},
    {"A", "S", "D", "F", "G", "H", "J", "K", "L", ".", "/"},
    {"Z", "X", "C", "V", "B", "N", "M", ":", "-", "DEL", "CLR"},
    {"SHIFT", "SPACE", "SPACE", "SPACE", "http://", ":8787", "OK", "OK", "CANCEL", "CANCEL", ""},
};

const int kModalX = 80;
const int kModalY = 62;
const int kModalW = 800;
const int kModalH = 420;
const int kGridX = 104;
const int kGridY = 176;
const int kKeyW = 64;
const int kKeyH = 50;
const int kKeyGap = 4;

const char* key_label(int page, int r, int c) {
    return (page == 0) ? kKeysLower[r][c] : kKeysUpper[r][c];
}

} // namespace

void OnScreenKeyboard::open(const std::string& title, const std::string& initial_text, size_t max_len) {
    title_ = title;
    buffer_ = initial_text;
    max_len_ = max_len;
    row_ = 1;
    col_ = 0;
    page_ = 0;
    open_ = true;
}

void OnScreenKeyboard::close() {
    open_ = false;
}

void OnScreenKeyboard::activate_key(int r, int c) {
    const char* k = key_label(page_, r, c);
    if (!k || k[0] == '\0') return;

    if (std::strcmp(k, "DEL") == 0) {
        if (!buffer_.empty()) buffer_.pop_back();
    } else if (std::strcmp(k, "CLR") == 0) {
        buffer_.clear();
    } else if (std::strcmp(k, "SHIFT") == 0) {
        page_ = (page_ == 0) ? 1 : 0;
    } else if (std::strcmp(k, "SPACE") == 0) {
        if (buffer_.size() < max_len_) buffer_.push_back(' ');
    } else if (std::strcmp(k, "OK") == 0) {
        open_ = false;
    } else if (std::strcmp(k, "CANCEL") == 0) {
        open_ = false;
    } else {
        size_t len = std::strlen(k);
        if (buffer_.size() + len <= max_len_) {
            buffer_ += k;
        }
    }
}

OskResult OnScreenKeyboard::update(uint32_t pressed_buttons, const SceTouchData& touch, bool touch_just_pressed) {
    if (!open_) return OskResult::Cancelled;

    if (pressed_buttons & SCE_CTRL_UP) {
        row_ = (row_ + kRows - 1) % kRows;
    }
    if (pressed_buttons & SCE_CTRL_DOWN) {
        row_ = (row_ + 1) % kRows;
    }
    if (pressed_buttons & SCE_CTRL_LEFT) {
        col_ = (col_ + kCols - 1) % kCols;
        if (std::strlen(key_label(page_, row_, col_)) == 0) col_ = kCols - 2;
    }
    if (pressed_buttons & SCE_CTRL_RIGHT) {
        col_ = (col_ + 1) % kCols;
        if (std::strlen(key_label(page_, row_, col_)) == 0) col_ = 0;
    }
    if (pressed_buttons & SCE_CTRL_SQUARE) {
        if (!buffer_.empty()) buffer_.pop_back();
    }
    if (pressed_buttons & SCE_CTRL_TRIANGLE) {
        page_ = (page_ == 0) ? 1 : 0;
    }
    if (pressed_buttons & SCE_CTRL_START) {
        open_ = false;
        return OskResult::Confirmed;
    }
    if (pressed_buttons & SCE_CTRL_CIRCLE) {
        open_ = false;
        return OskResult::Cancelled;
    }
    if (pressed_buttons & SCE_CTRL_CROSS) {
        const char* k = key_label(page_, row_, col_);
        if (std::strcmp(k, "OK") == 0) {
            open_ = false;
            return OskResult::Confirmed;
        }
        if (std::strcmp(k, "CANCEL") == 0) {
            open_ = false;
            return OskResult::Cancelled;
        }
        activate_key(row_, col_);
    }

    if (touch_just_pressed && touch.reportNum > 0) {
        int tx = touch.report[0].x / 2;
        int ty = touch.report[0].y / 2;
        for (int r = 0; r < kRows; ++r) {
            for (int c = 0; c < kCols; ++c) {
                const char* k = key_label(page_, r, c);
                if (!k || k[0] == '\0') continue;
                int x = kGridX + c * (kKeyW + kKeyGap);
                int y = kGridY + r * (kKeyH + kKeyGap);
                if (tx >= x && tx <= x + kKeyW && ty >= y && ty <= y + kKeyH) {
                    row_ = r;
                    col_ = c;
                    if (std::strcmp(k, "OK") == 0) {
                        open_ = false;
                        return OskResult::Confirmed;
                    }
                    if (std::strcmp(k, "CANCEL") == 0) {
                        open_ = false;
                        return OskResult::Cancelled;
                    }
                    activate_key(r, c);
                    return OskResult::Active;
                }
            }
        }
    }

    return OskResult::Active;
}

void OnScreenKeyboard::draw(vita2d_pgf* font) const {
    if (!open_) return;

    // Dim backdrop
    vita2d_draw_rectangle(0, 0, 960, 544, RGBA8(0, 0, 0, 195));
    // Modal card
    vita2d_draw_rectangle(kModalX, kModalY, kModalW, kModalH, RGBA8(24, 26, 34, 255));
    vita2d_draw_rectangle(kModalX, kModalY, kModalW, 3, RGBA8(225, 48, 108, 255));

    vita2d_pgf_draw_text(font, kModalX + 24, kModalY + 32, RGBA8(245, 245, 250, 255), 1.05f, title_.c_str());

    // Text input box
    vita2d_draw_rectangle(kModalX + 24, kModalY + 52, kModalW - 48, 42, RGBA8(14, 15, 20, 255));
    std::string shown = buffer_ + "_";
    if (shown.size() > 54) {
        shown = "..." + shown.substr(shown.size() - 51);
    }
    vita2d_pgf_draw_text(font, kModalX + 36, kModalY + 80, RGBA8(255, 255, 255, 255), 1.0f, shown.c_str());

    // Keys
    for (int r = 0; r < kRows; ++r) {
        for (int c = 0; c < kCols; ++c) {
            const char* k = key_label(page_, r, c);
            if (!k || k[0] == '\0') continue;

            int x = kGridX + c * (kKeyW + kKeyGap);
            int y = kGridY + r * (kKeyH + kKeyGap);
            bool selected = (r == row_ && c == col_);

            unsigned int bg = RGBA8(42, 46, 58, 255);
            if (std::strcmp(k, "OK") == 0) bg = RGBA8(0, 149, 246, 255);
            else if (std::strcmp(k, "CANCEL") == 0) bg = RGBA8(130, 42, 62, 255);
            if (selected) bg = RGBA8(225, 48, 108, 255);

            vita2d_draw_rectangle(x, y, kKeyW, kKeyH, bg);
            float scale = (std::strlen(k) > 3) ? 0.72f : 0.92f;
            int tw = vita2d_pgf_text_width(font, scale, k);
            int tx = x + std::max(4, (kKeyW - tw) / 2);
            int ty = y + 32;
            vita2d_pgf_draw_text(font, tx, ty, RGBA8(255, 255, 255, 255), scale, k);
        }
    }

    vita2d_pgf_draw_text(
        font, kModalX + 24, kModalY + kModalH - 12, RGBA8(170, 175, 190, 255), 0.78f,
        "X: Key   []: Backspace   /\\: Shift   START: Confirm   O: Cancel");
}

} // namespace vitagram

