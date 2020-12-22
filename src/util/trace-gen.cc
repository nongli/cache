#include "util/trace-gen.h"

#include <string>
#include <vector>

using namespace cache;
using namespace std;

vector<Request> TraceGen::SameKeyTrace(
      int64_t n, string_view k, string_view v) {
  vector<Request> result;
  for (int i = 0; i < n; ++i) {
    result.push_back(Request(k, v));
  }
  return result;
}
