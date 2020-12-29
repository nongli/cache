#pragma once

#include <cassert>
#include <iostream>
#include <map>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "cache/cache.h"
#include "util/trace-gen.h"

namespace cache {

// Sort decreasing
struct Oldest {
  bool operator()(const int64_t a, const int64_t b) const {
    return a > b;
  }
};

// Oracle cache using belady's algorithm
template <typename K, typename V>
class BeladyCache : public Cache<K, V> {
public:
  // The exact trace that will be run. This assumes that get() is called once for each
  // element in the trace.
  BeladyCache(int64_t size, Trace* trace) : _max_size(size) {
    create_cache_index(trace);
  }

  inline int64_t max_size() const { return _max_size; }
  inline int64_t size() const { return _cache.size(); }
  const Stats& stats() const { return _stats; }
  inline int64_t p() const { return 0; }
  inline int64_t max_p() const { return 0; }
  inline int64_t filter_size() const { return 0; }

  // Get value from the cache. If found bumps the element up in the LRU list.
  std::shared_ptr<V> get(const K& key) {
    assert(_access_by_key.find(key) != _access_by_key.end());
    AccessHistory& history = _access_by_key[key];
    // Remove the current access, it is used up
    int64_t t = history.access_order[history.idx++];

    auto elt = _cache.find(key);
    if (elt != _cache.end()) {
      if (_debug) {
        std::cout << "HIT(t=" << t << "): key="<< key << " cache-size="
                  << _cache.size() << std::endl;
      }
      ++_stats.num_hits;
      update_eviction_order(key, t, history);
      return elt->second;
    } else {
      if (_debug) {
        std::cout << "MISS(t=" << t << "): key="<< key << " cache-size="
                  << _cache.size() << std::endl;
      }
      ++_stats.num_misses;
      update_eviction_order(key, t, history);
      return nullptr;
    }
  }

  void add_to_cache(const K& key, std::shared_ptr<V> value) {
    if (_cache.size() >= _max_size) {
      ++_stats.num_evicted;
      evict();
    }
    assert(_cache.size() < _max_size);
    _cache[key] = value;

    AccessHistory& history = _access_by_key[key];
    if (history.idx < history.access_order.size()) {
      int64_t t = history.access_order[history.idx];
      _farthest_access[t]= key;
      if (_debug) {
        std::cout << "Adding key=" << key<< " to eviction history, next t="
                  << t << std::endl;
      }
    } else {
      _unused.insert(key);
      if (_debug) {
        std::cout << "Key=" << key<< " will not be used again" << std::endl;
      }
    }
  }

  void clear() {
    reset();
    _stats.clear();
  }

  // Resets the caching state to be the beginning of the trace. Note that for this
  // to work, the trace and the gets() have to be in lock step.
  void reset() {
    _farthest_access.clear();
    _unused.clear();
    _cache.clear();
    for (auto& kv: _access_by_key) {
      kv.second.idx = 0;
    }
  }

private:
  struct AccessHistory {
    // The logical time access order
    std::vector<int64_t> access_order;

    // The idx into access_order for the next access.
    int idx = 0;
  };

  // Walks the trace and crecates the indexing structure
  void create_cache_index(Trace* trace) {
    trace->Reset();
    int64_t t = 0;
    while (true) {
      const Request* r = trace->next();
      if (r == nullptr) {
        break;
      }
      _access_by_key[r->key].access_order.push_back(t++);
    }

    reset();
    trace->Reset();
  }

  // Evict an entry to fit
  void evict() {
    if (!_unused.empty()) {
      const K& key = *_unused.begin();
      if (_debug) {
        std::cout << "Evicting key=" << key
                  << " which will not be used again: " << std::endl;
      }
      _cache.erase(key);
      _unused.erase(key);
      return;
    }

    assert(!_farthest_access.empty());
    auto first = _farthest_access.begin();
    if (_debug) {
      std::cout << "Evicting key=" << first->second << " which will be used at t="
                << first->first << std::endl;
    }
    _cache.erase(first->second);
    _farthest_access.erase(first->first);
  }

  void update_eviction_order(const K& key, int64_t t, AccessHistory& history) {
    // Remove the current access, it is used up
    if (_farthest_access.find(t) != _farthest_access.end()) {
      _farthest_access.erase(t);
      if (history.idx < history.access_order.size()) {
        // Add back the next access on this key
        _farthest_access[history.access_order[history.idx]] = key;
        if (_debug) {
          std::cout << "Adding key=" << key<< " to eviction history, next t="
                    << history.access_order[history.idx] << std::endl;
        }
      } else {
        // This key is not seen again, add to a separate tracking list. We will evict
        // from this first.
        _unused.insert(key);
        if (_debug) {
          std::cout << "Key=" << key<< " will not be used again" << std::endl;
        }
      }
    }
  }

  // Cache
  int64_t _max_size;
  std::unordered_map<K, std::shared_ptr<V>> _cache;
  Stats _stats;

  // For each key, the access history generated from the trace
  std::unordered_map<K, AccessHistory> _access_by_key;

  // The priority queue of next access time to key with that time.
  std::map<int64_t, K, Oldest> _farthest_access;

  // Keys that are no longer used
  std::unordered_set<K> _unused;

  // If true, turn on some debug logging
  bool _debug = false;
};

}
