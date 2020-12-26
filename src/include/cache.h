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

template <typename K, typename V>
class Cache {
 public:
  Cache() {
  }

  virtual ~Cache() {
  }
};

}
