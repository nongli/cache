#include "util/belady.h"
#include "gtest/gtest.h"

using namespace cache;
using namespace std;

void TestTrace(BeladyCache<string, int64_t>* cache, Trace* trace) {
  while (true) {
    const Request* r = trace->next();
    if (r == nullptr)
      break;

    shared_ptr<int64_t> val = cache->get(r->key);
    if (!val) {
      cache->add_to_cache(r->key, make_shared<int64_t>(r->value));
    }
  }
}

TEST(BeladyTest, Basic) {
  // Trace goes 0..10, twice on a cache of 5. A typical cache would miss all
  // the time but this should hit 25%
  FixedTrace trace(TraceGen::CycleTrace(20, 10, 42));
  BeladyCache<string, int64_t> cache(5, &trace);

  TestTrace(&cache, &trace);
  ASSERT_EQ(5, cache.stats().num_hits);
  ASSERT_EQ(15, cache.stats().num_misses);
  ASSERT_EQ(10, cache.stats().num_evicted);

  trace.Reset();
  cache.reset();
  TestTrace(&cache, &trace);
  ASSERT_EQ(10, cache.stats().num_hits);
  ASSERT_EQ(30, cache.stats().num_misses);
  ASSERT_EQ(20, cache.stats().num_evicted);
}

