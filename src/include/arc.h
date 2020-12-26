#pragma once

/*
 * Implements a ARC cache.
 */

#include <algorithm>
#include <cassert>
#include <memory>
#include <mutex>

#include "include/cache.h"
#include "include/lru.h"
#include "include/stats.h"
#include "util/lock.h"
#include "util/nop-lock.h"

namespace cache {

template <typename K, typename V, typename Lock = WordLock>
class AdaptiveCache : public Cache<K, V> {
public:
  AdaptiveCache(int64_t size)
      : _max_size{size}, _lru_cache{size}, _lfu_cache{size}, _lru_ghost{size},
        _lfu_ghost{size} {}

  inline int64_t max_size() const { return _max_size; }
  inline int64_t size() const { return _lru_cache.size() + _lfu_cache.size(); }
  const Stats& stats() const { return _stats; }
  inline int64_t p() const { return _p; }
  inline int64_t max_p() const { return _max_p; }

  // Add an item to the cache. The difference here is we try to use existing
  // information to decide if the item was previously cached.
  void add_to_cache(const K& key, std::shared_ptr<V> value) {
    std::lock_guard<Lock> l(_lock);
    // Check if the key is already in LRU cache.
    // We do so by removing the item since well that is what we would do
    // eventually anyways.
    if (_lru_cache.remove_from_cache(key)) {
      // Given it was already in the LRU cache, we need to add it
      // to the lfu cache and call it a day.
      // No evict is safe here since we are removing from LRU moving
      // to LFU.
      _lfu_cache.add_to_cache_no_evict(key, value);
    } else if (_lfu_cache.get(key)) {
      // Just update the item, and don't worry about it.
      _lfu_cache.add_to_cache_no_evict(key, value);
    } else if (_lru_ghost.contains(key)) {
      // We used to have this key, we recently evicted it, let us make this
      // a frequent key. Case II in Figure 4.
      adapt_lru_ghost_hit();
      // Make space.
      replace(false);
      // Add to LFU cache
      _lfu_cache.add_to_cache_no_evict(key, value);
      _lru_ghost.remove_from_cache(key);
    } else if (_lfu_ghost.contains(key)) {
      // Case III
      adapt_lfu_ghost_hit();
      // Make space.
      replace(true);
      _lfu_cache.add_to_cache_no_evict(key, value);
      _lfu_ghost.remove_from_cache(key);
    } else {
      // Case IV
      int64_t lru_size = _lru_cache.size() + _lru_ghost.size();
      int64_t total_size = _lfu_cache.size() + _lfu_ghost.size() + lru_size;
      if (lru_size == _max_size) {
        if (_lru_cache.size() < _max_size) {
          // IV(a)
          _lru_ghost.evict_entry();
          replace(false);
        } else {
          auto key = _lru_cache.evict_entry(); // Make space.
          if (key) {
            _lru_ghost.add_to_cache(*key, nullptr);
            _stats.lru_evicts++;
            _stats.num_evicted++;
          }
        }
      } else if (lru_size < _max_size && total_size >= _max_size) {
        // IV(b)
        if (total_size == 2 * _max_size) {
          _lfu_ghost.evict_entry();
        }
        replace(false);
      }
      // FIXME: This is a weird place to end up, but not sure why.
      if (size() >= _max_size) {
        replace(false);
      }
      _lru_cache.add_to_cache_no_evict(key, value);
    }
    assert(_lfu_cache.size() + _lru_cache.size() <= _max_size);
  }

  // Update a cached element if it exists, do nothing otherwise. Boolean returns
  // whether or not value was updated.
  bool update_cache(const K& key, std::shared_ptr<V> value) {
    std::lock_guard<Lock> l(_lock);
    if (_lru_cache.contains(key)) {
      // Given it was already in the LRU cache, we need to add it
      // to the lfu cache and call it a day.
      // No evict is safe here since we are removing from LRU moving
      // to LFU.
      _lru_cache.remove_from_cache(key);
      _lfu_cache.add_to_cache_no_evict(key, value);
      return true;
    } else {
      return _lfu_cache.update_cache(key, value);
    }
  }

  // Get an item from the cache. This is one half of what the ARC paper does.
  std::shared_ptr<V> get(const K& key) {
    std::lock_guard<Lock> l(_lock);
    auto value = _lfu_cache.get(key);
    if (!value) {
      if ((value = _lru_cache.remove_from_cache(key))) {
        std::shared_ptr<V> insertable(value);
        _lfu_cache.add_to_cache_no_evict(key, insertable);
        ++_stats.num_hits;
        ++_stats.lru_hits;
      } else {
        ++_stats.num_misses;
        // Access ghosts.
        bool lru_ghost = _lru_ghost.contains(key);
        bool lfu_ghost = _lfu_ghost.contains(key);
        _stats.lfu_ghost_hits += (int64_t)lfu_ghost;
        _stats.lfu_ghost_hits += (int64_t)lru_ghost;
        assert((!(lru_ghost || lfu_ghost)) || (lru_ghost ^ lfu_ghost));
      }
    } else {
      ++_stats.num_hits;
      ++_stats.lfu_hits;
    }
    assert(_lfu_cache.size() + _lru_cache.size() <= _max_size);
    return value;
  }

  // Remove key from the cache.
  std::shared_ptr<V> remove_from_cache(const K& key) {
    std::lock_guard<Lock> l(_lock);
    auto value = _lru_cache.remove_from_cache(key);
    if (value) {
      return value;
    } else if ((value = _lfu_cache.remove_from_cache(key))) {
      return value;
    }
    _lru_ghost.remove_from_cache(key);
    _lfu_ghost.remove_from_cache(key);
    return value;
  }

  void clear() {
    std::lock_guard<Lock> l(_lock);
    _stats.clear();
    _lru_cache.clear();
    _lfu_cache.clear();
    _lru_ghost.clear();
    _lfu_ghost.clear();
    _p = 0;
  }

  AdaptiveCache() = delete;
  AdaptiveCache(const AdaptiveCache&) = delete;
  AdaptiveCache operator=(const AdaptiveCache&) = delete;

protected:
  inline void adapt_lru_ghost_hit() {
    int64_t delta = 0;
    if (_lru_ghost.size() >= _lfu_ghost.size()) {
      delta = 1;
    } else {
      delta = (_lfu_ghost.size() / _lru_ghost.size());
    }
    _p = std::min(_p + delta, _max_size);
    _max_p = std::max(_max_p, _p);
  }

  inline void adapt_lfu_ghost_hit() {
    int64_t delta = 0;
    if (_lfu_ghost.size() >= _lru_ghost.size()) {
      delta = 1;
    } else {
      delta = _lru_ghost.size() / _lfu_ghost.size();
    }
    _p = std::max(_p - delta, int64_t(0));
    // Don't need to update _max_p here
  }

  inline void replace(bool in_lfu_ghost) {
    if (_lru_cache.size() > 0 && ((_lru_cache.size() > _p) ||
                                  (_lru_cache.size() == _p && in_lfu_ghost))) {
      std::optional<K> evicted = _lru_cache.evict_entry();
      if (evicted) {
        _lru_ghost.add_to_cache(*evicted, nullptr);
        ++_stats.lru_evicts;
      } else {
        --_stats.num_evicted;
      }
    } else {
      if (_lfu_cache.size() > 0) {
        std::optional<K> evicted = _lfu_cache.evict_entry();
        assert(evicted);
        _lfu_ghost.add_to_cache(*evicted, nullptr);
        ++_stats.lfu_evicts;
      } else {
        // OK this is a weird situation to be. In general we expect that each
        // call to replace removes at least one element, but what if LFU has
        // nothing to evict and LRU is ignored due to current p?
        if (_lru_cache.size() >= _max_size) {
          std::optional<K> evicted = _lru_cache.evict_entry();
          assert(evicted);
          _lru_ghost.add_to_cache(*evicted, nullptr);
          ++_stats.lru_evicts;
        } else {
          assert(_lru_cache.size() + _lfu_cache.size() < _max_size);
          --_stats.num_evicted;
        }
      }
    }
    ++_stats.num_evicted;
  }

private:
  Lock _lock;
  int64_t _max_size;
  int64_t _p = 0;
  int64_t _max_p = 0;
  LRUCache<K, V, NopLock> _lru_cache;
  LRUCache<K, V, NopLock> _lfu_cache;
  LRUCache<K, V, NopLock> _lru_ghost;
  LRUCache<K, V, NopLock> _lfu_ghost;
  Stats _stats;
};

} // namespace cache
