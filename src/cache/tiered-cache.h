#pragma once

/*
 * Implements a tiered cache that puts keys based on size.
 * FIXME: everything about this needs to be thought through
 */

#include <algorithm>
#include <cassert>
#include <memory>
#include <mutex>
#include <vector>

#include "cache/cache.h"

namespace cache {

template <typename K, typename V, typename C, typename Lock = NopLock,
          typename Sizer = ElementCount<V>>
class TieredCache : public Cache<K, V> {
public:
  TieredCache() {}

  inline int64_t max_size() const { return _max_size; }

  inline int64_t size() const {
    int64_t s = 0;
    for (C* c: _caches) {
      s += c->size();
    }
    return s;
  }

  inline int64_t num_entries() const {
    int64_t s = 0;
    for (auto c: _caches) {
      s += c->num_entries();
    }
    return s;
  }

  inline int64_t p() const {
    int64_t s = 0;
    for (auto c: _caches) {
      s += c->p();
    }
    return s;
  }

  inline int64_t max_p() const {
    int64_t s = 0;
    for (auto c: _caches) {
      s = std::max(s, c->max_p());
    }
    return s;
  }

  // TODO: crazy expensive
  const Stats& stats() {
    _stats.clear();
    for (auto c: _caches) {
      _stats.merge(c->stats());
    }
    return _stats;
  }

  void clear() {
    std::lock_guard<Lock> l(_lock);
    for (auto c: _caches) {
      c->clear();
    }
  }

  void reset() {
    std::lock_guard<Lock> l(_lock);
    for (auto c: _caches) {
      c->reset();
    }
  }

  inline int64_t filter_size() const { return 0; }
  inline Lock* get_lock() { return &_lock; }

  const std::string label(int64_t n) const {
    return "tiered-" + std::to_string(max_size() * 100 / n);
  }

  std::shared_ptr<V> get(const K& key) {
    std::lock_guard<Lock> l(_lock);
    for (auto c: _caches) {
      std::shared_ptr<V> v = c->get(key);
      if (v) return v;
    }
    return std::shared_ptr<V>(nullptr);
  }

  void add_to_cache(const K& key, std::shared_ptr<V> value) {
    std::lock_guard<Lock> l(_lock);
    int64_t size = _sizer(value.get());
    for (int i = 0; i < _sizes.size(); ++i) {
      if (size <= _sizes[i]) {
        _caches[i]->add_to_cache(key, value);
        return;
      }
    }
  }

  // Adds a cache to the tiering. Values up to max_size are put in this cache.
  void add_cache(int64_t max_size, std::shared_ptr<C> cache) {
    if (!_sizes.empty()) {
      assert(max_size > _sizes[_sizes.size() - 1]);
    }
    _sizes.push_back(max_size);
    _caches.push_back(cache);
    _max_size += cache->max_size();
  }

  TieredCache(const TieredCache&) = delete;
  TieredCache operator=(const TieredCache&) = delete;

private:
  Lock _lock;
  Sizer _sizer;
  int64_t _max_size = 0;

  std::vector<int64_t> _sizes;
  std::vector<std::shared_ptr<C>> _caches;
  Stats _stats;
};

}
