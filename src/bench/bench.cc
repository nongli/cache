#include "gflags/gflags.h"
#include "include/flex-arc.h"
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
void print_csv_line(int64_t cache_size, int64_t ghost_size,
                    const cache::Stats& stats) {
  std::cout << cache_size << "," << ghost_size << "," << stats.num_hits << ","
            << stats.num_misses << "," << stats.num_evicted << std::endl;
}

static bool validate_cache_parameters(const char*, int64_t value) {
  return value > 0;
}

DEFINE_bool(ghost_size, true,
            "Vary the size of ghost variables and measure cache efficacy.");
DEFINE_int64(cache_size, 25,
             "Size of cache to use. Note that we have 850 unique values");
DEFINE_validator(cache_size, &validate_cache_parameters);
DEFINE_double(zipf_parameter, 0.8, "Zipf parameter");
DEFINE_int64(ghost_begin, 2,
             "Minimum Ghost size to start with. Must be larger than 1.");
DEFINE_validator(ghost_begin, &validate_cache_parameters);
DEFINE_int64(ghost_end, 75, "Maximum ghost size to test.");
DEFINE_validator(ghost_end, &validate_cache_parameters);
DEFINE_int64(ghost_increment, 5, "How much to increase parameter by.");
DEFINE_validator(ghost_increment, &validate_cache_parameters);

void vary_ghost_size() {
  cache::FixedTrace trace(cache::TraceGen::ZipfianDistribution(
      42, 10000, 850, FLAGS_zipf_parameter, "value"));
  for (int64_t gs = FLAGS_ghost_begin; gs <= FLAGS_ghost_end;
       gs += FLAGS_ghost_increment) {
    const auto stats = TestTrace(
        cache::FlexARC<std::string, std::string>(FLAGS_cache_size, gs), &trace);
    print_csv_line(FLAGS_cache_size, gs, stats);
  }
}
int main(int argc, char* argv[]) {
  gflags::SetUsageMessage("Cache Benchmarking");
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  print_csv_header();
  if (FLAGS_ghost_size) {
    vary_ghost_size();
  }
}
