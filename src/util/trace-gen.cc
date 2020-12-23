#include "util/trace-gen.h"

#include <random>
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

vector<Request> TraceGen::NormalDistribution(int64_t n, double mean, double stddev,
    string_view v) {
  vector<Request> result;

  random_device rd{};
  mt19937 gen{rd()};

  // values near the mean are the most likely
  // standard deviation affects the dispersion of generated values from the mean
  normal_distribution<> d{mean,stddev};
  for(int64_t i = 0; i < n; ++i) {
    result.push_back(Request(std::to_string(std::round(d(gen))), v));
  }

  return result;
}

void FixedTrace::Add(const vector<Request>& trace) {
  for (const Request& r: trace) {
    _requests.push_back(r);
  }
}
