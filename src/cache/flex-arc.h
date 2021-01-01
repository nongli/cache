#pragma once

/*
 * Implements a ARC cache.
 */

#include <algorithm>
#include <cassert>
#include <memory>
#include <mutex>

#include "cache/cache.h"
#include "cache/lru.h"

namespace cache {
template <typename K, typename V, typename Lock = NopLock,
          typename Sizer = ElementCount<V>>
class FlexARC : public Cache<K, V> {
public:
  // Produces an ARC with ghost lists of size ghost_size, and cache of size
  // size.
  FlexARC(int64_t size, int64_t ghost_size, int64_t filter_size = 0)
      : _max_size{size}, _p{0}, _max_p{0}, _ghost_size{ghost_size},
        _lru_cache{size}, _lfu_cache{size}, _lru_ghost{ghost_size},
        _lfu_ghost{ghost_size}, _filter(filter_size) {}

  inline int64_t max_size() const { return _max_size; }
  inline int64_t size() const { return _lru_cache.size() + _lfu_cache.size(); }
  inline int64_t num_entries() const {
    return _lru_cache.num_entries() + _lfu_cache.num_entries();
  }
  inline int64_t ghost_size() const { return _ghost_size; }
  const Stats& stats() const { return _stats; }
  inline int64_t p() const { return _p; }
  inline int64_t max_p() const { return _max_p; }
  inline int64_t filter_size() const { return _filter.max_size(); }
  inline Lock* get_lock() { return &_lock; }

  const std::string label(int64_t n) const {
    return "farc-" + std::to_string(max_size() * 100 / n) + "-" +
           std::to_string(ghost_size() * 100 / max_size());
  }

  // Add an item to the cache. The difference here is we try to use existing
  // information to decide if the item was previously cached.
  void add_to_cache(const K& key, std::shared_ptr<V> value) {
    std::lock_guard<Lock> l(_lock);
    bool lru_ghost_hit = _lru_ghost.contains(key);
    bool lfu_ghost_hit = _lfu_ghost.contains(key);
    bool in_lfu = false;
    bool should_replace = true;

    // Check if the key is already in LRU cache.
    // We do so by removing the item since well that is what we would do
    // eventually anyways.
    if (_lru_cache.contains(key)) {
      // Given it was already in the LRU cache, we need to add it
      // to the lfu cache and call it a day.
      _lru_cache.remove_from_cache(key);
      _lfu_cache.add_to_cache_no_evict(key, value);
      assert(!_lru_ghost.contains(key) && !_lfu_ghost.contains(key));
      in_lfu = false;
    } else if (_lfu_cache.contains(key)) {
      // Just update the item, and don't worry about it.
      _lfu_cache.add_to_cache_no_evict(key, value);
      assert(!_lru_ghost.contains(key) && !_lfu_ghost.contains(key));
      // Now we might need to make space.
      in_lfu = true;
    } else if (!(lfu_ghost_hit || lru_ghost_hit) && _filter.max_size() > 0 &&
               !_filter.contains(key)) {
      // Filter should only kick in for entries evicted far enough in the past.
      // Add a "double-hit" pre filter. This is intended to prevent single scan
      // keys from invalidating the cache.
      ++_stats.arc_filter;
      _filter.add_to_cache(key, nullptr);
      // Do not call replace in this case
      should_replace = false;
    } else if (lru_ghost_hit) {
      // We used to have this key, we recently evicted it, let us make this
      // a frequent key. Case II in Figure 4.
      adapt_lru_ghost_hit();
      // Add things back
      _lfu_cache.add_to_cache_no_evict(key, value);
      _lru_ghost.remove_from_cache(key);
      assert(!_lru_ghost.contains(key) && !_lfu_ghost.contains(key));
      // Do this only after fixing all invariants, to evict.
      in_lfu = false;
    } else if (lfu_ghost_hit) {
      // Case III
      adapt_lfu_ghost_hit();
      // Add things
      _lfu_cache.add_to_cache_no_evict(key, value);
      _lfu_ghost.remove_from_cache(key);
      assert(!_lru_ghost.contains(key) && !_lfu_ghost.contains(key));
      in_lfu = true;
    } else {
      // Case IV
      assert(!_lru_ghost.contains(key) && !_lfu_ghost.contains(key));
      _lru_cache.add_to_cache_no_evict(key, value);
      in_lfu = false;
    }
    // The call to replace restores the size invariant.
    if (should_replace) {
      replace(in_lfu);
    }
    assert(_lfu_cache.size() + _lru_cache.size() <= _max_size);
  }

  // Update a cached element if it exists, do nothing otherwise. Boolean returns
  // whether or not value was updated.
  // FIXME: THIS DOES NOT CURRENTLY HANDLE SIZERS.
  bool update_cache(const K& key, std::shared_ptr<V> value) {
    std::lock_guard<Lock> l(_lock);
    if (_lru_cache.contains(key)) {
      // Given it was already in the LRU cache, we need to add it
      // to the lfu cache and call it a day.
      // No evict is safe here since we are removing from LRU moving
      // to LFU.
      _lru_cache.remove_from_cache(key);
      _lfu_cache.add_to_cache_no_evict(key, value);
      replace(false);
      return true;
    } else if (_lfu_cache.contains(key)) {
      _lfu_cache.update_cache(key, value);
      replace(false);
      return true;
    } else {
      return false;
    }
  }

  // Get an item from the cache. This is one half of what the ARC paper does.
  std::shared_ptr<V> get(const K& key) {
    std::lock_guard<Lock> l(_lock);
    std::shared_ptr<V> lfu_value = _lfu_cache.get(key);
    if (lfu_value) {
      ++_stats.num_hits;
      ++_stats.lfu_hits;
      assert(_lfu_cache.size() + _lru_cache.size() <= _max_size);
      return lfu_value;
    }

    std::shared_ptr<V> lru_value = _lru_cache.remove_from_cache(key);
    if (lru_value) {
      _lfu_cache.add_to_cache_no_evict(key, lru_value);
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
    assert(_lfu_cache.size() + _lru_cache.size() <= _max_size);
    return lru_value;
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

  void reset() {
    std::lock_guard<Lock> l(_lock);
    _lru_cache.clear();
    _lfu_cache.clear();
    _lru_ghost.clear();
    _lfu_ghost.clear();
    _filter.clear();
    _p = 0;
  }

  void clear() {
    _stats.clear();
    reset();
  }

  // Set the maximum cache size.
  void set_max_size(int64_t size) {
    std::lock_guard<Lock> l(_lock);
    if (size < _max_size) {
      if (_p > size) {
        // p must be between 0 and _max_size, but what this is telling us is
        // that we should be dedicating all of the cache space to LRU. So let us
        // just do that.
        // NOTE[apanda]: There is an argument to be made for proportional
        // reduction but I am not sure it makes sense, so might need to revisit.
        _p = size;
        _max_size = size;
        replace(false);
      }
    } else {
      _max_size = size;
      // Do not need to adjust anything.
    }
  }

  FlexARC() = delete;
  FlexARC(const FlexARC&) = delete;
  FlexARC operator=(const FlexARC&) = delete;

protected:
  inline void adapt_lru_ghost_hit() {
    int64_t delta = 0;
    if (_lru_ghost.size() >= _lfu_ghost.size()) {
      delta = 1;
    } else {
      assert(_lru_ghost.size() != 0);
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
      assert(_lru_ghost.size() != 0);
      delta = _lru_ghost.size() / _lfu_ghost.size();
    }
    _p = std::max(_p - delta, int64_t(0));
    // Don't need to update _max_p here
  }

  inline void replace(bool in_lfu_ghost) {
    // Avoid unnecessary evictions.
    while (_lru_cache.size() + _lfu_cache.size() > _max_size) {
      size_t bytes_evicted = 0;
      if (_lru_cache.size() > 0 &&
          ((_lru_cache.size() > _p) ||
           (_lru_cache.size() == _p && in_lfu_ghost))) {
        std::optional<K> evicted = _lru_cache.evict_entry(bytes_evicted);
        if (evicted) {
          _lru_ghost.add_to_cache(*evicted, nullptr);
          assert(!_lfu_ghost.contains(*evicted) &&
                 !_lru_cache.contains(*evicted));
          ++_stats.lru_evicts;
          _stats.bytes_evicted += bytes_evicted;
        }
      } else if (_lfu_cache.size() > 0) {
        std::optional<K> evicted = _lfu_cache.evict_entry(bytes_evicted);
        if (evicted) {
          _lfu_ghost.add_to_cache(*evicted, nullptr);
          assert(!_lru_ghost.contains(*evicted));
          ++_stats.lfu_evicts;
          _stats.bytes_evicted += bytes_evicted;
        }
      } else {
        // We need to evict something, so...
        std::optional<K> evicted = _lru_cache.evict_entry(bytes_evicted);
        if (evicted) {
          _lru_ghost.add_to_cache(*evicted, nullptr);
          assert(!_lfu_ghost.contains(*evicted) &&
                 !_lru_cache.contains(*evicted));
          ++_stats.lru_evicts;
          _stats.bytes_evicted += bytes_evicted;
        }
      }
      ++_stats.num_evicted;
    }
  }

private:
  Lock _lock;
  int64_t _max_size;
  int64_t _p;
  int64_t _max_p;
  int64_t _ghost_size;
  LRUCache<K, V, NopLock, Sizer> _lru_cache;
  LRUCache<K, V, NopLock, Sizer> _lfu_cache;
  LRUCache<K, V, NopLock> _lru_ghost;
  LRUCache<K, V, NopLock> _lfu_ghost;
  LRUCache<K, V, NopLock> _filter;
  Stats _stats;
};
} // namespace cache
