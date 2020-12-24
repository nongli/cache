#pragma once

/*
 * Implements a ARC cache.
 */

#include <algorithm>
#include <cassert>
#include <memory>
#include <tuple>

#include "include/cache.h"
#include "include/lru.h"
#include "include/stats.h"

namespace cache {
struct Hello {};
template <typename K, typename V> class FlexARC : public Cache<K, V> {

public:
  // Produces an ARC with ghost lists of size ghost_size, and cache of size
  // size.
  FlexARC(size_t size, size_t ghost_size)
      : _max_size{size}, _p{0}, _ghost_size{ghost_size}, _lru_cache{size},
        _lfu_cache{size}, _lru_ghost{ghost_size}, _lfu_ghost{ghost_size} {}

  inline size_t size() const { return _lru_cache.size() + _lfu_cache.size(); }
  const Stats& stats() const { return _stats; }

  FlexARC() = delete;
  FlexARC(const FlexARC&) = delete;
  FlexARC operator=(const FlexARC&) = delete;
  // Add an item to the cache. The difference here is we try to use existing
  // information to decide if the item was previously cached.
  void add_to_cache(const K& key, std::shared_ptr<V> value) {
    // Check if the key is already in LRU cache.
    // We do so by removing the item since well that is what we would do
    // eventually anyways.
    if (_lru_cache.remove_from_cache(key)) {
      // Given it was already in the LRU cache, we need to add it
      // to the lfu cache and call it a day.
      _lfu_cache.add_to_cache_no_evict(key, value);
      assert(!_lru_ghost.contains(key) && !_lfu_ghost.contains(key));
    } else if (_lfu_cache.get(key)) {
      // Just update the item, and don't worry about it.
      _lfu_cache.add_to_cache_no_evict(key, value);
      assert(!_lru_ghost.contains(key) && !_lfu_ghost.contains(key));
    } else if (_lru_ghost.contains(key)) {
      // We used to have this key, we recently evicted it, let us make this
      // a frequent key. Case II in Figure 4.
      adapt_lru_ghost_hit();
      _lfu_cache.add_to_cache_no_evict(key, value);
      _lru_ghost.remove_from_cache(key);
      assert(!_lru_ghost.contains(key) && !_lfu_ghost.contains(key));
      // Do this only after fixing all invariants
      replace(false);
    } else if (_lfu_ghost.contains(key)) {
      // Case III
      adapt_lfu_ghost_hit();
      _lfu_cache.add_to_cache_no_evict(key, value);
      _lfu_ghost.remove_from_cache(key);
      assert(!_lru_ghost.contains(key) && !_lfu_ghost.contains(key));
      replace(true);
    } else {
      // Case IV
      size_t lru_size = _lru_cache.size();
      size_t total_size = _lfu_cache.size() + lru_size;
      _lru_cache.add_to_cache(key, value);
      assert(!_lru_ghost.contains(key) && !_lfu_ghost.contains(key));
      if (lru_size == _max_size) {
        // We are using the entire LRU cache, we need to evict items
        // in order to make space.
        auto evicted = _lru_cache.evict_entry();
        _lru_ghost.add_to_cache(evicted, nullptr);
        assert(!_lfu_ghost.contains(evicted) && !_lru_cache.contains(evicted));
        _stats.lru_evicts++;
        _stats.num_evicted++;
      } else if (total_size >= _max_size) {
        // IV(b)
        replace(false);
      }
    }
    assert(_lfu_cache.size() + _lru_cache.size() <= _max_size);
  }

  // Get an item from the cache. This is one half of what the ARC paper does.
  std::shared_ptr<V> get(const K& key) {
    auto value = _lfu_cache.get(key);
    if (!value) {
      if ((value = _lru_cache.remove_from_cache(key))) {
        std::shared_ptr<V> insertable(value);
        _lfu_cache.add_to_cache_no_evict(key, insertable);
        assert(!_lru_ghost.contains(key) && !_lfu_ghost.contains(key));
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
      assert(!_lfu_ghost.contains(evicted) && !_lru_cache.contains(evicted));
      ++_stats.lru_evicts;
    } else if (_lfu_cache.size() > 0) {
      K evicted = _lfu_cache.evict_entry();
      _lfu_ghost.add_to_cache(evicted, nullptr);
      assert(!_lru_ghost.contains(evicted));
      ++_stats.lfu_evicts;
    } else {
      // We need to evict something, so...
      K evicted = _lru_cache.evict_entry();
      _lru_ghost.add_to_cache(evicted, nullptr);
      assert(!_lfu_ghost.contains(evicted) && !_lru_cache.contains(evicted));
      ++_stats.lru_evicts;
    }
    ++_stats.num_evicted;
  }

private:
  size_t _max_size;
  size_t _p;
  size_t _ghost_size;
  LRUCache<K, V> _lru_cache;
  LRUCache<K, V> _lfu_cache;
  LRUCache<K, V> _lru_ghost;
  LRUCache<K, V> _lfu_ghost;

  Stats _stats;
};
} // namespace cache
