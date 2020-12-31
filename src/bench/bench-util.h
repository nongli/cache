#pragma once

#include <chrono>
#include <iostream>
#include <string>

#include "cache/cache.h"
#include "cache/flex-arc.h"
#include "util/table-printer.h"
#include "util/trace-gen.h"

namespace cache {

enum class CacheType { Lru, Arc, Farc, Belady, Tiered };

inline int64_t ParseMemSpec(const std::string& mem_spec_str) {
  if (mem_spec_str.empty()) return 0;

  int64_t multiplier = -1;
  int32_t number_str_len = mem_spec_str.size();

  // Look for an accepted suffix such as "MB" or "M".
  std::string::const_reverse_iterator suffix_char = mem_spec_str.rbegin();
  if (*suffix_char == 'b' || *suffix_char == 'B') {
    // Skip "B", the default is bytes anyways.
    if (suffix_char == mem_spec_str.rend()) return -1;
    suffix_char++;
    number_str_len--;
  }
  switch (*suffix_char) {
    case 'g':
    case 'G':
      // Gigabytes.
      number_str_len--;
      multiplier = 1024L * 1024L * 1024L;
      break;
    case 'm':
    case 'M':
      // Megabytes.
      number_str_len--;
      multiplier = 1024L * 1024L;
      break;
  }

  int64_t bytes;
  if (multiplier != -1) {
    // Parse float - MB or GB
    double limit_val = stod(std::string(mem_spec_str.data(), number_str_len));
    bytes = multiplier * limit_val;
  } else {
    bytes = stol(std::string(mem_spec_str.data(), number_str_len));
  }
  return bytes;
}

template <class Cache>
inline void Run(TablePrinter* results, int64_t n, const std::string& name,
                Trace* trace, Cache* cache, CacheType type, int iters) {
  std::string label;
  switch (type) {
  case CacheType::Arc:
    if (cache->filter_size() > 0) {
      label = "arc-" + std::to_string(cache->max_size() * 100 / n) + "-filter";
    } else {
      label = "arc-" + std::to_string(cache->max_size() * 100 / n);
    }
    break;
  case CacheType::Lru:
    label = "lru-" + std::to_string(cache->max_size() * 100 / n);
    break;
  case CacheType::Farc:
    label =
        "farc-" + std::to_string(cache->max_size() * 100 / n) + "-" +
        std::to_string(
            dynamic_cast<FlexARC<std::string, int64_t, NopLock, TraceSizer>*>(cache)
                ->ghost_size() *
            100 / cache->max_size());
    break;
  case CacheType::Belady:
    label = "belady-" + std::to_string(cache->max_size() * 100 / n);
    break;
  case CacheType::Tiered:
    label = "tiered-" + std::to_string(cache->max_size() * 100 / n);
    break;
  default:
    assert(false);
  }
  std::cerr << "Testing adaptive cache (" << label << ") on trace " << name << std::endl;

  cache->clear();

  int64_t total_vals = 0;
  double total_micros = 0;
  for (int i = 0; i < iters; ++i) {
    trace->Reset();
    cache->reset();
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    while (true) {
      const Request* r = trace->next();
      if (r == nullptr) {
        break;
      }
      std::shared_ptr<int64_t> val = cache->get(r->key);
      ++total_vals;
      if (total_vals % 2500000 == 0) {
        std::cerr << "   ...tested " << total_vals << " values" << std::endl;
      }
      if (!val) {
        cache->add_to_cache(r->key, std::make_shared<int64_t>(r->value));
      }
    }
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    total_micros +=
        std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  }
  std::cerr << "    Completed in  " << total_micros / 1000 << " ms" << std::endl;

  Stats stats = cache->stats();
  std::vector<std::string> row;
  row.push_back(name);
  row.push_back(label);
  row.push_back(std::to_string(stats.num_hits));
  row.push_back(std::to_string(stats.num_misses));
  row.push_back(std::to_string(stats.num_evicted));
  if (type == CacheType::Lru) {
    row.push_back("-");
    row.push_back("-");
  } else {
    row.push_back(std::to_string(cache->p()));
    row.push_back(std::to_string(cache->max_p()));
  }
  row.push_back(
      std::to_string(stats.num_hits * 100 / (stats.num_hits + stats.num_misses)));
  if (type == CacheType::Lru) {
    row.push_back("-");
    row.push_back("-");
  } else {
    if (stats.num_hits > 0) {
      row.push_back(std::to_string(stats.lru_hits * 100 / stats.num_hits));
      row.push_back(std::to_string(stats.lfu_hits * 100 / stats.num_hits));
    } else {
      row.push_back("-");
      row.push_back("-");
    }
  }
  row.push_back(
      std::to_string(stats.num_misses * 100 / (stats.num_hits + stats.num_misses)));
  if (type == CacheType::Lru) {
    row.push_back("-");
    row.push_back("-");
  } else {
    if (stats.num_misses > 0) {
      row.push_back(std::to_string(stats.lru_ghost_hits * 100 / stats.num_misses));
      row.push_back(std::to_string(stats.lfu_ghost_hits * 100 / stats.num_misses));
    } else {
      row.push_back("-");
      row.push_back("-");
    }
  }
  if (stats.arc_filter > 0) {
    row.push_back(std::to_string(stats.arc_filter));
  } else {
    row.push_back("-");
  }
  row.push_back(std::to_string(total_micros / total_vals));

  results->AddRow(row);
}


}
