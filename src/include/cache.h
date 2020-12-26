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

  void clear() {
    memset(this, 0, sizeof(Stats));
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

template <typename K, typename V>
class Cache {
 public:
  Cache() {
  }

  virtual ~Cache() {
  }
};

}
