#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace vitagram {

struct JsonValue;
using JsonArray = std::vector<JsonValue>;
using JsonObject = std::map<std::string, JsonValue>;

struct JsonValue {
    using Variant = std::variant<std::nullptr_t, bool, double, std::string, JsonArray, JsonObject>;
    Variant data{nullptr};

    bool is_null() const { return std::holds_alternative<std::nullptr_t>(data); }
    bool is_bool() const { return std::holds_alternative<bool>(data); }
    bool is_number() const { return std::holds_alternative<double>(data); }
    bool is_string() const { return std::holds_alternative<std::string>(data); }
    bool is_array() const { return std::holds_alternative<JsonArray>(data); }
    bool is_object() const { return std::holds_alternative<JsonObject>(data); }

    std::string as_string(const std::string& fallback = "") const;
    int64_t as_int(int64_t fallback = 0) const;
    bool as_bool(bool fallback = false) const;
    const JsonArray& as_array() const;
    const JsonObject& as_object() const;

    const JsonValue& get(const std::string& key) const;
};

bool parse_json(const std::string& input, JsonValue& out, std::string& error);

} // namespace vitagram

