#include "cache/arc.h"
#include "util/compare.h"
#include "util/trace-gen.h"
#include "gtest/gtest.h"

using namespace cache;
using namespace std;

template <class Cache> Stats TestTrace(Cache cache, Trace* trace) {
  trace->Reset();
  while (true) {
    const Request* r = trace->next();
    if (r == nullptr)
      break;
    shared_ptr<int64_t> val = cache.get(r->key);
    if (!val) {
      cache.add_to_cache(r->key, make_shared<int64_t>(r->value));
    }
  }
  return cache.stats();
}

TEST(CacheCompare, Zipf) {
  ResultCompare compare;
  FixedTrace trace(TraceGen::ZipfianDistribution(42, 10000, 500, 1, 4));

  compare.AddResult("arc-100",
                    TestTrace(AdaptiveCache<string, int64_t>(100), &trace));
  compare.AddResult("arc-50",
                    TestTrace(AdaptiveCache<string, int64_t>(50), &trace));
  compare.AddResult("lru-100",
                    TestTrace(LRUCache<string, int64_t>(100), &trace));
  compare.AddResult("lru-50", TestTrace(LRUCache<string, int64_t>(50), &trace));

  printf("%s", compare.Report("zipf-500-1").c_str());
}

TEST(CacheCompare, ZipfLongSequence) {
  int N = 50000;
  ResultCompare compare;
  InterleavdTrace trace;
  FixedTrace zipf = FixedTrace(TraceGen::ZipfianDistribution(42, N, N, .7, 4));
  // All unique keys
  FixedTrace big = FixedTrace(TraceGen::CycleTrace(N, N, 4));
  trace.Add(&zipf);

  compare.AddResult("arc-10%",
                    TestTrace(AdaptiveCache<string, int64_t>(N * .1), &trace));
  compare.AddResult("arc-5%",
                    TestTrace(AdaptiveCache<string, int64_t>(N * .05), &trace));
  compare.AddResult("lru-10%",
                    TestTrace(LRUCache<string, int64_t>(N * .1), &trace));
  compare.AddResult("lru-5%", TestTrace(LRUCache<string, int64_t>(N * .05), &trace));

  printf("%s", compare.Report("zipf-long-seq").c_str());
}

TEST(CacheCompare, ZipfMediumCycle) {
  int N = 50000;
  ResultCompare compare;
  InterleavdTrace trace;
  FixedTrace zipf = FixedTrace(TraceGen::ZipfianDistribution(42, N, N, .7, 4));
  // All unique keys
  FixedTrace big = FixedTrace(TraceGen::CycleTrace(N, N / 5, 4));
  trace.Add(&zipf);

  compare.AddResult("arc-10%",
                    TestTrace(AdaptiveCache<string, int64_t>(N * .1), &trace));
  compare.AddResult("arc-5%",
                    TestTrace(AdaptiveCache<string, int64_t>(N * .05), &trace));
  compare.AddResult("lru-10%",
                    TestTrace(LRUCache<string, int64_t>(N * .1), &trace));
  compare.AddResult("lru-5%", TestTrace(LRUCache<string, int64_t>(N * .05), &trace));

  printf("%s", compare.Report("zipf-long-seq").c_str());
}
