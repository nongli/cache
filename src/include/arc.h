#pragma once
/*
 * Implements a ARC cache.
 */
#include "lru.h"
#include <algorithm>
#include <cassert>
#include <memory>
#include <tuple>

namespace cache {
template <typename K, typename V> class AdaptiveCache {
public:
  // FIXME: We should use different sizes for all of the caches.
  // FIXME: Ghost size should be a separate parameter, since it might enable
  // some optimizations.
  AdaptiveCache(size_t size)
      : _max_size{size}, _lru_cache{size}, _lfu_cache{size}, _lru_ghost{size},
        _lfu_ghost{size}, _p{0} {}
  AdaptiveCache() = delete;
  AdaptiveCache(const AdaptiveCache &) = delete;
  inline size_t size() const { return _lru_cache.size() + _lfu_cache.size(); }

protected:
  inline void adapt_lru_ghost_hit() {
    size_t delta = 0;
    if (_lru_ghost.size() >= _lfu_ghost.size()) {
      delta = 1;
    } else {
      delta = (_lfu_ghost.size() / _lru_ghost.size());
    }
    _p = std::min(_p + delta, _max_size);
  }

  inline void adapt_lfu_ghost_hit() {
    size_t delta = 0;
    if (_lfu_ghost.size() >= _lru_ghost.size()) {
      delta = 1;
    } else {
      delta = _lru_ghost.size() / _lfu_ghost.size();
    }
    _p = std::max(_p - delta, size_t(0));
  }

  inline void replace(bool in_lfu_ghost) {
    if (_lru_cache.size() > 0 && ((_lru_cache.size() > _p) ||
                                  (_lru_cache.size() == _p && in_lfu_ghost))) {
      K evicted = _lru_cache.evict_entry();
      _lru_ghost.add_to_cache(evicted, nullptr);
    } else {
      K evicted = _lfu_cache.evict_entry();
      _lfu_ghost.add_to_cache(evicted, nullptr);
    }
  }

public:
  void add_to_cache(const K &key, std::shared_ptr<V> value) {
    // Check if the key is already in LRU cache.
    // We do so by removing the item since well that is what we would do
    // eventually anyways.
    if (_lru_cache.remove_from_cache(key)) {
      // Given it was already in the LRU cache, we need to add it
      // to the lfu cache and call it a day.
      _lfu_cache.add_to_cache(key, value);
    } else if (_lfu_cache.get(key)) {
      // Just update the item, and don't worry about it.
      _lfu_cache.add_to_cache(key, value);
    } else if (_lru_ghost.contains(key)) {
      // We used to have this key, we recently evicted it, let us make this
      // a frequent key. Case II in Figure 4.
      adapt_lru_ghost_hit();
      replace(false);
      _lfu_cache.add_to_cache(key, value);
      _lru_ghost.remove_from_cache(key);
    } else if (_lfu_ghost.contains(key)) {
      // Case III
      adapt_lfu_ghost_hit();
      replace(true);
      _lfu_cache.add_to_cache(key, value);
      _lfu_ghost.remove_from_cache(key);
    } else {
      // Case IV
      size_t lru_size = _lru_cache.size() + _lru_ghost.size();
      size_t total_size = _lfu_cache.size() + _lfu_ghost.size() + lru_size;
      if (lru_size == _max_size) {
        // IV(a)
        if (_lru_cache.size() < _max_size) {
          _lru_ghost.evict_entry();
          replace(false);
        } else {
          _lru_cache.evict_entry(); // Make space.
        }
      } else if (lru_size < _max_size && total_size >= _max_size) {
        if (total_size == 2 * _max_size) {
          _lfu_ghost.evict_entry();
        }
        replace(false);
      }
      _lru_cache.add_to_cache(key, value);
    }
  }

  std::shared_ptr<V> get(const K &key) {
    auto value = _lfu_cache.get(key);
    if (!value) {
      if ((value = _lru_cache.remove_from_cache(key))) {
        std::shared_ptr<V> insertable(value);
        _lfu_cache.add_to_cache(key, insertable);
      } else {
        // Access ghosts.
        bool lru_ghost = _lru_ghost.contains(key);
        bool lfu_ghost = _lfu_ghost.contains(key);
        assert((!(lru_ghost || lfu_ghost)) || (lru_ghost ^ lfu_ghost));
      }
    }
    return value;
  }

  AdaptiveCache operator=(const AdaptiveCache &) = delete;

private:
  size_t _max_size;
  size_t _p;
  LRUCache<K, V> _lru_cache;
  LRUCache<K, V> _lfu_cache;
  LRUCache<K, V> _lru_ghost;
  LRUCache<K, V> _lfu_ghost;
};
} // namespace cache
