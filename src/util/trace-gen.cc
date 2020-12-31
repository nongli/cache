#include "util/trace-gen.h"

#include <iostream>
#include <random>
#include <string>
#include <vector>

using namespace cache;
using namespace std;

Trace::~Trace() {}

FixedTrace::FixedTrace(const vector<Request>& trace) : _requests(trace) {}

Zipfian::Zipfian(int64_t n, double alpha) {
  double c = 0;
  for (int64_t i = 1; i <= n; ++i) {
    c += (1.0 / pow((double)i, alpha));
  }
  c = 1.0 / c;

  _sum_probs.resize(n + 1);
  _sum_probs[0] = 0;
  for (int64_t i = 1; i <= n; ++i) {
    _sum_probs[i] = _sum_probs[i - 1] + c / pow((double)i, alpha);
  }
}

int64_t Zipfian::Gen() {
  double z;
  do {
    z = (double)rand() / RAND_MAX;
  } while (z == 0 || z == 1);

  int64_t low = 1;
  int64_t high = _sum_probs.size() - 1;
  int64_t mid;
  do {
    mid = floor((low + high) / 2);
    if (_sum_probs[mid] >= z && _sum_probs[mid - 1] < z) {
      return mid;
    } else if (_sum_probs[mid] >= z) {
      high = mid - 1;
    } else {
      low = mid + 1;
    }
  } while (low <= high);
  return 0;
}

vector<Request> TraceGen::SameKeyTrace(int64_t n, string_view k,
                                       int64_t v, MemoryPool* pool) {
  vector<Request> result;
  for (int i = 0; i < n; ++i) {
    char* ext = pool == nullptr ? nullptr : pool->allocate_and_copy(k);
    result.push_back(Request(k, v, ext));
  }
  return result;
}

vector<Request> TraceGen::CycleTrace(int64_t n, int64_t k, int64_t v,
                                     MemoryPool* pool) {
  vector<Request> result;
  for (int64_t i = 0; i < n; ++i) {
    string key(std::to_string(i % k));
    char* ext = pool == nullptr ? nullptr : pool->allocate_and_copy(key);
    result.push_back(Request(key, v, ext));
  }
  return result;
}

const Request* InterleavdTrace::next() {
  while (true) {
    if (_active_traces.empty()) return nullptr;
    int trace_idx = rand() % _active_traces.size();
    const Request* r = _active_traces[trace_idx]->next();
    if (r != nullptr) return r;
    _active_traces.erase(_active_traces.begin() + trace_idx);
  }
  return nullptr;
}

void InterleavdTrace::Add(Trace* trace) {
  _active_traces.push_back(trace);
  _traces.push_back(trace);
}

void InterleavdTrace::Reset() {
  _active_traces = _traces;
  for (Trace* t: _active_traces) {
    t->Reset();
  }
}

template <class Distribution>
vector<Request> Gen(int64_t n, Distribution& d, int64_t v, MemoryPool* pool) {
  vector<Request> result;
  random_device rd{};
  mt19937 gen{rd()};
  for (int64_t i = 0; i < n; ++i) {
    string key = std::to_string(std::round(d(gen)));
    char* ext = pool == nullptr ? nullptr : pool->allocate_and_copy(key);
    result.push_back(Request(key, v, ext));
  }
  return result;
}

vector<Request> TraceGen::NormalDistribution(int64_t n, double mean,
                                             double stddev, int64_t v,
                                             MemoryPool* pool) {
  normal_distribution<> d{mean, stddev};
  return Gen(n, d, v, pool);
}

vector<Request> TraceGen::PoissonDistribution(int64_t n, double mean,
                                              int64_t v,
                                              MemoryPool* pool) {
  poisson_distribution<> d{mean};
  return Gen(n, d, v, pool);
}

vector<Request> TraceGen::ZipfianDistribution(int64_t n, int64_t k,
                                              double alpha, int64_t v,
                                              MemoryPool* pool) {
  Zipfian zipf(k, alpha);
  vector<Request> result;
  for (int64_t i = 0; i < n; ++i) {
    string key = std::to_string(zipf.Gen());
    char* ext = pool == nullptr ? nullptr : pool->allocate_and_copy(key);
    result.push_back(Request(key, v, ext));
  }
#ifdef PRINT_TRACE
  for (const auto& req : result) {
    std::cout << req.key << std::endl;
  }
#endif
  return result;
}

vector<Request> TraceGen::ZipfianDistribution(uint32_t seed, int64_t n,
                                              int64_t k, double alpha,
                                              int64_t v,
                                              MemoryPool* pool) {
  srand(seed);
  return TraceGen::ZipfianDistribution(n, k, alpha, v, pool);
}

void FixedTrace::Add(const vector<Request>& trace) {
  for (const Request& r : trace) {
    _requests.push_back(r);
  }
}
