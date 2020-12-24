#include "util/compare.h"

#include <sstream>

using namespace std;

namespace cache {

void ResultCompare::AddResult(string_view label, Stats result) {
  _results[string(label)] = result;
}

string ResultCompare::Report(string_view title) {
  stringstream ss;
  ss << title << "\n";
  for (const auto& kv : _results) {
    ss << " " << kv.first << "\n";
    ss << "   hits: " << kv.second.num_hits << "\n";
    ss << "   misses: " << kv.second.num_misses << "\n";
    ss << "   evictions: " << kv.second.num_evicted << "\n";
    ss << "   LRU hits (ARC only): " << kv.second.lru_hits << "\n";
    ss << "   LRU evicts (ARC only): " << kv.second.lru_evicts << "\n";
    ss << "   LFU hits (ARC only): " << kv.second.lfu_hits << "\n";
    ss << "   LFU evicts (ARC only): " << kv.second.lfu_evicts << "\n";
    ss << "   LRU ghost hits (ARC only): " << kv.second.lru_ghost_hits << "\n";
    ss << "   LFU ghost hits (ARC only): " << kv.second.lfu_ghost_hits << "\n";
  }
  return ss.str();
}

} // namespace cache
