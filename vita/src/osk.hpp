#pragma once

#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <string>
#include <vita2d.h>

namespace vitagram {

enum class OskResult {
    Active,
    Confirmed,
    Cancelled,
};

class OnScreenKeyboard {
public:
    void open(const std::string& title, const std::string& initial_text, size_t max_len = 128);
    void close();
    bool is_open() const { return open_; }

    const std::string& text() const { return buffer_; }

    OskResult update(uint32_t pressed_buttons, const SceTouchData& touch, bool touch_just_pressed);
    void draw(vita2d_pgf* font) const;

private:
    bool open_ = false;
    std::string title_;
    std::string buffer_;
    size_t max_len_ = 128;
    int row_ = 1;
    int col_ = 0;
    int page_ = 0; // 0 = lowercase, 1 = uppercase, 2 = symbols/url

    void activate_key(int r, int c);
};

} // namespace vitagram

