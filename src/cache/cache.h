#pragma once

#include <cstdint>
#include <cstring>

namespace cache {

struct Stats {
  int64_t num_hits = 0;
  int64_t num_misses = 0;
  int64_t num_evicted = 0;
  int64_t lfu_hits = 0;
  int64_t lru_hits = 0;
  int64_t lfu_evicts = 0;
  int64_t lru_evicts = 0;
  int64_t lfu_ghost_hits = 0;
  int64_t lru_ghost_hits = 0;
  int64_t arc_filter = 0;

  void clear() { memset(this, 0, sizeof(Stats)); }
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

} // namespace cache
