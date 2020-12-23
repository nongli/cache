#pragma once

#include <cstdint>

namespace cache {

struct Stats {
  int64_t num_hits = 0;
  int64_t num_misses = 0;
  int64_t num_evicted = 0;
  int64_t lfu_hits = 0;
  int64_t lru_hits = 0;
  int64_t lfu_ghost_hits = 0;
  int64_t lru_ghost_hits = 0;
};

}
