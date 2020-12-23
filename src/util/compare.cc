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
  for (const auto& kv: _results) {
    ss << " " << kv.first << "\n";
    ss << "   hits: " << kv.second.num_hits << "\n";
    ss << "   misses: " << kv.second.num_misses << "\n";
  }
  return ss.str();
}

}
