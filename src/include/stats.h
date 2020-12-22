#pragma once

#include <cstdint>

namespace cache {

struct Stats {
  int64_t num_hits = 0;
  int64_t num_misses = 0;
};

}
