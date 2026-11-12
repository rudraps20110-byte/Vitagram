#include "json.hpp"

#include <cctype>
#include <cstdlib>

namespace vitagram {

namespace {

const JsonValue kNullValue{};
const JsonArray kEmptyArray{};
const JsonObject kEmptyObject{};

void append_utf8(std::string& out, uint32_t cp) {
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

class Parser {
public:
    explicit Parser(const std::string& src) : s_(src) {}

    bool parse(JsonValue& out, std::string& err) {
        skip_ws();
        if (!parse_value(out, err, 0)) return false;
        skip_ws();
        if (pos_ != s_.size()) {
            err = "Unexpected trailing characters in JSON";
            return false;
        }
        return true;
    }

private:
    const std::string& s_;
    size_t pos_ = 0;

    void skip_ws() {
        while (pos_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[pos_]))) {
            ++pos_;
        }
    }

    bool parse_value(JsonValue& out, std::string& err, int depth) {
        if (depth > 64) {
            err = "JSON nesting too deep";
            return false;
        }
        skip_ws();
        if (pos_ >= s_.size()) {
            err = "Unexpected end of JSON";
            return false;
        }
        char c = s_[pos_];
        if (c == 'n') return parse_literal("null", nullptr, out, err);
        if (c == 't') return parse_literal("true", true, out, err);
        if (c == 'f') return parse_literal("false", false, out, err);
        if (c == '"') {
            std::string str;
            if (!parse_string(str, err)) return false;
            out.data = std::move(str);
            return true;
        }
        if (c == '[') return parse_array(out, err, depth + 1);
        if (c == '{') return parse_object(out, err, depth + 1);
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parse_number(out, err);

        err = "Invalid JSON token";
        return false;
    }

    template <typename T>
    bool parse_literal(const char* lit, T val, JsonValue& out, std::string& err) {
        size_t i = 0;
        while (lit[i] != '\0') {
            if (pos_ + i >= s_.size() || s_[pos_ + i] != lit[i]) {
                err = std::string("Expected ") + lit;
                return false;
            }
            ++i;
        }
        pos_ += i;
        out.data = val;
        return true;
    }

    bool parse_hex4(uint32_t& code, std::string& err) {
        if (pos_ + 4 > s_.size()) {
            err = "Truncated unicode escape";
            return false;
        }
        code = 0;
        for (int i = 0; i < 4; ++i) {
            char c = s_[pos_++];
            code <<= 4;
            if (c >= '0' && c <= '9') code |= (c - '0');
            else if (c >= 'a' && c <= 'f') code |= (c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') code |= (c - 'A' + 10);
            else {
                err = "Invalid hex digit in unicode escape";
                return false;
            }
        }
        return true;
    }

    bool parse_string(std::string& out, std::string& err) {
        if (s_[pos_] != '"') {
            err = "Expected '\"'";
            return false;
        }
        ++pos_;
        out.clear();
        while (pos_ < s_.size()) {
            char c = s_[pos_++];
            if (c == '"') return true;
            if (c == '\\') {
                if (pos_ >= s_.size()) {
                    err = "Unterminated escape";
                    return false;
                }
                char e = s_[pos_++];
                switch (e) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    case 'u': {
                        uint32_t cp = 0;
                        if (!parse_hex4(cp, err)) return false;
                        if (cp >= 0xD800 && cp <= 0xDBFF && pos_ + 2 <= s_.size() &&
                            s_[pos_] == '\\' && s_[pos_ + 1] == 'u') {
                            pos_ += 2;
                            uint32_t low = 0;
                            if (!parse_hex4(low, err)) return false;
                            if (low >= 0xDC00 && low <= 0xDFFF) {
                                cp = 0x10000 + (((cp - 0xD800) << 10) | (low - 0xDC00));
                            }
                        }
                        append_utf8(out, cp);
                        break;
                    }
                    default:
                        err = "Unknown string escape";
                        return false;
                }
            } else {
                out.push_back(c);
            }
        }
        err = "Unterminated string";
        return false;
    }

    bool parse_number(JsonValue& out, std::string& err) {
        const char* start = s_.c_str() + pos_;
        char* end = nullptr;
        double val = std::strtod(start, &end);
        if (end == start) {
            err = "Invalid number";
            return false;
        }
        pos_ += static_cast<size_t>(end - start);
        out.data = val;
        return true;
    }

    bool parse_array(JsonValue& out, std::string& err, int depth) {
        ++pos_; // skip '['
        JsonArray arr;
        skip_ws();
        if (pos_ < s_.size() && s_[pos_] == ']') {
            ++pos_;
            out.data = std::move(arr);
            return true;
        }
        while (pos_ < s_.size()) {
            JsonValue elem;
            if (!parse_value(elem, err, depth)) return false;
            arr.push_back(std::move(elem));
            skip_ws();
            if (pos_ < s_.size() && s_[pos_] == ',') {
                ++pos_;
                continue;
            }
            if (pos_ < s_.size() && s_[pos_] == ']') {
                ++pos_;
                out.data = std::move(arr);
                return true;
            }
            err = "Expected ',' or ']' in array";
            return false;
        }
        err = "Unterminated array";
        return false;
    }

    bool parse_object(JsonValue& out, std::string& err, int depth) {
        ++pos_; // skip '{'
        JsonObject obj;
        skip_ws();
        if (pos_ < s_.size() && s_[pos_] == '}') {
            ++pos_;
            out.data = std::move(obj);
            return true;
        }
        while (pos_ < s_.size()) {
            skip_ws();
            std::string key;
            if (pos_ >= s_.size() || s_[pos_] != '"') {
                err = "Expected string key in object";
                return false;
            }
            if (!parse_string(key, err)) return false;
            skip_ws();
            if (pos_ >= s_.size() || s_[pos_] != ':') {
                err = "Expected ':' after object key";
                return false;
            }
            ++pos_;
            JsonValue val;
            if (!parse_value(val, err, depth)) return false;
            obj[std::move(key)] = std::move(val);
            skip_ws();
            if (pos_ < s_.size() && s_[pos_] == ',') {
                ++pos_;
                continue;
            }
            if (pos_ < s_.size() && s_[pos_] == '}') {
                ++pos_;
                out.data = std::move(obj);
                return true;
            }
            err = "Expected ',' or '}' in object";
            return false;
        }
        err = "Unterminated object";
        return false;
    }
};

} // namespace

std::string JsonValue::as_string(const std::string& fallback) const {
    if (auto* p = std::get_if<std::string>(&data)) return *p;
    return fallback;
}

int64_t JsonValue::as_int(int64_t fallback) const {
    if (auto* p = std::get_if<double>(&data)) return static_cast<int64_t>(*p);
    return fallback;
}

bool JsonValue::as_bool(bool fallback) const {
    if (auto* p = std::get_if<bool>(&data)) return *p;
    return fallback;
}

const JsonArray& JsonValue::as_array() const {
    if (auto* p = std::get_if<JsonArray>(&data)) return *p;
    return kEmptyArray;
}

const JsonObject& JsonValue::as_object() const {
    if (auto* p = std::get_if<JsonObject>(&data)) return *p;
    return kEmptyObject;
}

const JsonValue& JsonValue::get(const std::string& key) const {
    if (auto* p = std::get_if<JsonObject>(&data)) {
        auto it = p->find(key);
        if (it != p->end()) return it->second;
    }
    return kNullValue;
}

bool parse_json(const std::string& input, JsonValue& out, std::string& error) {
    Parser parser(input);
    return parser.parse(out, error);
}

} // namespace vitagram

