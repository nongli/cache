#include "cache/arc.h"
#include "gflags/gflags.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
void parse_trace(const std::string& trace) {
  std::ifstream tr(trace);
  std::string line;
  cache::AdaptiveCache<std::string, int64_t, cache::NopLock, cache::TraceSizer>
      cache(25ll * 1000ll * 1000ll * 1000ll * 100ll);
  int64_t iters = 0;
  while (std::getline(tr, line)) {
    std::stringstream l(line);
    std::string key;
    int64_t size;
    l >> key >> size;
    if (!cache.get(key)) {
      cache.add_to_cache(key, std::make_shared<int64_t>(size));
    }
    iters++;
    if (iters % 1000 == 0) {
      std::cout << cache.stats().num_hits << " " << cache.stats().num_misses
                << std::endl;
    }
  }
  std::cout << cache.stats().num_hits << " " << cache.stats().num_misses
            << std::endl;
}

DEFINE_string(trace, "", "Trace filename");
int main(int argc, char* argv[]) {
  gflags::SetUsageMessage("Trace reader");
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  if (FLAGS_trace.empty()) {
    std::cerr << "No trace specified\n";
    std::exit(1);
  }
  parse_trace(FLAGS_trace);
}
