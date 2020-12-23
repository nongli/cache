#include "include/arc.h"
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
    shared_ptr<string> val = cache.get(r->key);
    if (!val) {
      cache.add_to_cache(r->key, make_shared<string>(r->value));
    }
  }
  return cache.stats();
}

TEST(CacheCompare, Zipf) {
  ResultCompare compare;
  FixedTrace trace(TraceGen::ZipfianDistribution(42, 10000, 500, 1, "value"));

  compare.AddResult("arc-100",
                    TestTrace(AdaptiveCache<string, string>(100), &trace));
  compare.AddResult("arc-50",
                    TestTrace(AdaptiveCache<string, string>(50), &trace));
  compare.AddResult("lru-100",
                    TestTrace(LRUCache<string, string>(100), &trace));
  compare.AddResult("lru-50", TestTrace(LRUCache<string, string>(50), &trace));

  printf("n%s", compare.Report("zipf-500-1").c_str());
}
