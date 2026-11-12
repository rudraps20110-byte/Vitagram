#pragma once

#include <list>
#include <string>
#include <unordered_map>
#include <vita2d.h>

namespace vitagram {

class TextureCache {
public:
    explicit TextureCache(size_t max_entries = 24) : max_entries_(max_entries) {}
    ~TextureCache();

    TextureCache(const TextureCache&) = delete;
    TextureCache& operator=(const TextureCache&) = delete;

    vita2d_texture* get(const std::string& key);
    void put(const std::string& key, vita2d_texture* tex);
    bool has_failed(const std::string& key) const;
    void mark_failed(const std::string& key);
    void clear();

private:
    size_t max_entries_;
    std::list<std::pair<std::string, vita2d_texture*>> lru_;
    std::unordered_map<std::string, std::list<std::pair<std::string, vita2d_texture*>>::iterator> index_;
    std::unordered_map<std::string, bool> failed_;
};

} // namespace vitagram

