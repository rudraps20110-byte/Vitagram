#include "texture_cache.hpp"

namespace vitagram {

TextureCache::~TextureCache() {
    clear();
}

vita2d_texture* TextureCache::get(const std::string& key) {
    auto it = index_.find(key);
    if (it == index_.end()) return nullptr;
    lru_.splice(lru_.begin(), lru_, it->second);
    return it->second->second;
}

void TextureCache::put(const std::string& key, vita2d_texture* tex) {
    if (!tex) {
        mark_failed(key);
        return;
    }
    failed_.erase(key);

    auto it = index_.find(key);
    if (it != index_.end()) {
        if (it->second->second != tex) {
            vita2d_wait_rendering_done();
            vita2d_free_texture(it->second->second);
            it->second->second = tex;
        }
        lru_.splice(lru_.begin(), lru_, it->second);
        return;
    }

    lru_.emplace_front(key, tex);
    index_[key] = lru_.begin();

    while (lru_.size() > max_entries_) {
        auto last = std::prev(lru_.end());
        vita2d_wait_rendering_done();
        vita2d_free_texture(last->second);
        index_.erase(last->first);
        lru_.pop_back();
    }
}

bool TextureCache::has_failed(const std::string& key) const {
    return failed_.find(key) != failed_.end();
}

void TextureCache::mark_failed(const std::string& key) {
    failed_[key] = true;
}

void TextureCache::clear() {
    if (!lru_.empty()) {
        vita2d_wait_rendering_done();
        for (auto& entry : lru_) {
            if (entry.second) vita2d_free_texture(entry.second);
        }
    }
    lru_.clear();
    index_.clear();
    failed_.clear();
}

} // namespace vitagram

