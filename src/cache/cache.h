#pragma once

#include <cstdint>
#include <cstring>
#include <string>

namespace cache {

struct Stats {
  int64_t num_hits = 0;
  int64_t num_misses = 0;
  int64_t num_evicted = 0;
  int64_t bytes_hit = 0;
  int64_t bytes_evicted = 0;
  int64_t lfu_hits = 0;
  int64_t lru_hits = 0;
  int64_t lfu_evicts = 0;
  int64_t lru_evicts = 0;
  int64_t lfu_ghost_hits = 0;
  int64_t lru_ghost_hits = 0;
  int64_t arc_filter = 0;

  void clear() { memset(this, 0, sizeof(Stats)); }

  void merge(const Stats& s) {
    num_hits += s.num_hits;
    num_misses += s.num_misses;
    num_evicted += s.num_evicted;
    bytes_hit += s.bytes_hit;
    bytes_evicted += s.bytes_evicted;
    lfu_hits += s.lfu_hits;
    lru_hits += s.lru_hits;
    lfu_evicts += s.lfu_evicts;
    lru_evicts += s.lru_evicts;
    lfu_ghost_hits += s.lfu_ghost_hits;
    lru_ghost_hits += s.lru_ghost_hits;
    arc_filter += s.arc_filter;
  }
};

// A nop lock for when fine-grained locking in LRU makes no sense since coarse
// grained locking is used externally.
class NopLock final {
public:
  constexpr NopLock() = default;
  constexpr void lock() {}
  constexpr void unlock() {}
  constexpr bool try_lock() { return true; }
};

template <typename K, typename V> class Cache {
public:
  Cache() {}

  virtual ~Cache() {}
};

// Count number of elements
template <typename V> class ElementCount {
public:
  ElementCount() {}
  ElementCount(const ElementCount&) = default;
  ElementCount(ElementCount&&) = default;
  inline int64_t operator()(const V*) const { return 1; }
};

// Count by size
template <typename V> class ValueSize {
public:
  ValueSize() {}
  ValueSize(const ValueSize&) = default;
  ValueSize(ValueSize&&) = default;
  inline int64_t operator()(const V* v) const {
    if (v) {
      return sizeof(V);
    } else {
      return 0;
    }
  }
};

// Count by size
class StringSizer {
public:
  StringSizer() {}
  StringSizer(const StringSizer&) = default;
  StringSizer(StringSizer&&) = default;
  inline int64_t operator()(const std::string* v) const {
    if (v) {
      return v->size();
    } else {
      return 0;
    }
  }
};

// This sizer uses the value (which it assumes is a int64_t as the reported
// size. This makes it easier to handle traces with sizes without needing
// values.
class TraceSizer {
public:
  TraceSizer() {}
  TraceSizer(const TraceSizer&) = default;
  TraceSizer(TraceSizer&&) = default;
  inline int64_t operator()(const int64_t* v) const {
    if (v) {
      return *v;
    } else {
      return 0;
    }
  }
};

} // namespace cache
