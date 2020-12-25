#include "include/tunable-arc.h"
#include "util/compare.h"
#include "util/trace-gen.h"
#include <iostream>

template <class Cache>
cache::Stats TestTrace(Cache cache, cache::Trace* trace) {
  trace->Reset();
  while (true) {
    const cache::Request* r = trace->next();
    if (r == nullptr)
      break;
    std::shared_ptr<std::string> val = cache.get(r->key);
    if (!val) {
      cache.add_to_cache(r->key, std::make_shared<std::string>(r->value));
    }
  }
  return cache.stats();
}

void print_csv_header() {
  std::cout << "Cache Size,Ghost Size,Hits,Misses,Evicted" << std::endl;
}
void print_csv_line(size_t cache_size, size_t ghost_size,
                    const cache::Stats& stats) {
  std::cout << cache_size << "," << ghost_size << "," << stats.num_hits << ","
            << stats.num_misses << "," << stats.num_evicted << std::endl;
}
int main(int argc, char* argv[]) {
  cache::FixedTrace trace(
      cache::TraceGen::ZipfianDistribution(42, 10000, 850, 0.8, "value"));
  const size_t CACHE_SIZE = 25;
  const size_t MIN_GHOST_SIZE = 2;
  const size_t MAX_GHOST_SIZE = 3 * CACHE_SIZE;
  const size_t GHOST_INCREMENT = 5;
  print_csv_header();
  for (size_t gs = MIN_GHOST_SIZE; gs <= MAX_GHOST_SIZE;
       gs += GHOST_INCREMENT) {
    const auto stats = TestTrace(
        cache::FlexARC<std::string, std::string>(CACHE_SIZE, gs), &trace);
    print_csv_line(CACHE_SIZE, gs, stats);
  }
}
