#include "util/trace-gen.h"

#include <string>
#include <vector>

using namespace cache;
using namespace std;

Trace::~Trace() {
}

FixedTrace::FixedTrace(const vector<Request>& trace) : _requests(trace) {
}

vector<Request> TraceGen::SameKeyTrace(
      int64_t n, string_view k, string_view v) {
  vector<Request> result;
  for (int i = 0; i < n; ++i) {
    result.push_back(Request(k, v));
  }
  return result;
}

vector<Request> TraceGen::CycleTrace(int64_t n, int64_t k, string_view v) {
  vector<Request> result;
  for (int64_t i = 0; i < n; ++i) {
    int64_t key = i % k;
    result.push_back(Request(std::to_string(key), v));
  }
  return result;
}
