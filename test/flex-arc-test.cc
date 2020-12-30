#include "cache/flex-arc.h"
#include "util/trace-gen.h"
#include "gtest/gtest.h"

using namespace cache;
using namespace std;

TEST(FlexArc, SmallCache) {
  FlexARC<string, string> cache(2, 4);
  ASSERT_EQ(cache.size(), 0);
  cache.add_to_cache("Baby Yoda", make_shared<string>("Unknown Name"));
  ASSERT_EQ(cache.size(), 1);
  cache.add_to_cache("Baby Yoda", make_shared<string>("Grogu"));
  ASSERT_EQ(cache.size(), 1);
  const string& val = *cache.get("Baby Yoda");
  ASSERT_TRUE(val == "Grogu");
  cache.add_to_cache("The Mandalorian", make_shared<string>("Din Djarin"));
  ASSERT_EQ(cache.size(), 2);
  cache.add_to_cache("Bounty Hunter", make_shared<string>("Boba Fett"));
  ASSERT_EQ(cache.size(), 2);
  ASSERT_EQ(cache.get("The Mandalorian"), nullptr);

  shared_ptr<string> p = cache.remove_from_cache("Baby Yoda");
  ASSERT_EQ(p.use_count(), 1);
  ASSERT_TRUE(*p == "Grogu");
  ASSERT_EQ(cache.size(), 1);
  ASSERT_EQ(cache.get("Baby Yoda"), nullptr);
}

TEST(FlexArc, SmallCacheSized) {
  FlexARC<string, string, NopLock, StringSizer> cache(16, 4);
  ASSERT_EQ(cache.size(), 0);
  cache.add_to_cache("K0", make_shared<string>("Abcd"));
  ASSERT_EQ(cache.size(), 4);
  cache.add_to_cache("K0", make_shared<string>("Abcde"));
  ASSERT_EQ(cache.size(), 5);
  cache.add_to_cache("K0", make_shared<string>("012345678901234567"));
  ASSERT_EQ(cache.size(), 0);
  cache.add_to_cache("K0", make_shared<string>("0123"));
  cache.add_to_cache("K1", make_shared<string>("01234"));
  cache.add_to_cache("K2", make_shared<string>("012345"));
  const string& v = *cache.get("K1");
  ASSERT_TRUE(v == "01234");
  cache.add_to_cache("K3", make_shared<string>("012"));
  ASSERT_EQ(cache.size(), 12);
}

TEST(FlexArc, LRUOnly) {
  FlexARC<string, string> cache(2, 2);
  ASSERT_EQ(cache.size(), 0);
  cache.add_to_cache("Baby Yoda", make_shared<string>("Unknown Name"));
  ASSERT_EQ(cache.size(), 1);
  cache.add_to_cache("The Mandalorian", make_shared<string>("Din Djarin"));
  ASSERT_EQ(cache.size(), 2);
  cache.add_to_cache("Bounty Hunter", make_shared<string>("Boba Fett"));
  ASSERT_EQ(cache.size(), 2);
  ASSERT_EQ(cache.get("Baby Yoda"), nullptr);
}

TEST(FlexArc, Adaptive) {
  FlexARC<string, string> cache(2, 2);
  ASSERT_EQ(cache.size(), 0);
  cache.add_to_cache("Baby Yoda", make_shared<string>("Unknown Name"));
  ASSERT_EQ(cache.size(), 1);
  // Push to LFU side
  const string& val = *cache.get("Baby Yoda");
  ASSERT_TRUE(val == "Unknown Name");
  // Adds to LRU
  cache.add_to_cache("The Mandalorian", make_shared<string>("Din Djarin"));
  ASSERT_EQ(cache.size(), 2);
  // Adds to LRU
  cache.add_to_cache("Bounty Hunter", make_shared<string>("Boba Fett"));
  ASSERT_EQ(cache.size(), 2);
  // Trigger adaptation
  cache.add_to_cache("The Mandalorian", make_shared<string>("Din Djarin"));
  ASSERT_EQ(cache.size(), 2);
  ASSERT_EQ(cache.get("Baby Yoda"), nullptr);
}

void TestTrace(FlexARC<string, int64_t>* cache, Trace* trace) {
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

TEST(FlexArc, SingleKey) {
  FlexARC<string, int64_t> cache(2, 2);
  FixedTrace trace(TraceGen::SameKeyTrace(100, "key", 4));
  TestTrace(&cache, &trace);
  ASSERT_EQ(99, cache.stats().num_hits);
  ASSERT_EQ(1, cache.stats().num_misses);
}

TEST(FlexArc, AllUniqueKey) {
  FlexARC<string, int64_t> cache(100, 100);
  FixedTrace trace(TraceGen::CycleTrace(100, 100, 4));
  TestTrace(&cache, &trace);
  ASSERT_EQ(0, cache.stats().num_hits);
  ASSERT_EQ(100, cache.stats().num_misses);
}

TEST(FlexArc, SmallCycle) {
  FlexARC<string, int64_t> cache(100, 100);
  FixedTrace trace(TraceGen::CycleTrace(100, 20, 4));
  TestTrace(&cache, &trace);
  ASSERT_EQ(80, cache.stats().num_hits);
  ASSERT_EQ(20, cache.stats().num_misses);
}

TEST(FlexArc, Gaussian) {
  // This will fail with some probability. Retry if this is a problem?
  FlexARC<string, int64_t> cache(100, 100);
  FixedTrace trace(TraceGen::NormalDistribution(500, 20, 5, 4));
  TestTrace(&cache, &trace);
  ASSERT_GT(cache.stats().num_hits, 400);
  ASSERT_LT(cache.stats().num_misses, 100);

  FlexARC<string, int64_t> cache2(100, 100);
  FixedTrace trace2(TraceGen::NormalDistribution(500, 1000, 100, 4));
  TestTrace(&cache2, &trace2);
  ASSERT_GT(cache2.stats().num_hits, 50);
  ASSERT_LT(cache2.stats().num_misses, 450);
}

TEST(FlexArc, Poisson) {
  // This will fail with some probability. Retry if this is a problem?
  FlexARC<string, int64_t> cache(100, 100);
  FixedTrace trace(TraceGen::PoissonDistribution(500, 20, 4));
  TestTrace(&cache, &trace);
  ASSERT_GT(cache.stats().num_hits, 400);
  ASSERT_LT(cache.stats().num_misses, 100);
}

TEST(FlexArc, Zipf) {
  // This will fail with some probability. Retry if this is a problem?
  FlexARC<string, int64_t> cache(100, 100);
  FixedTrace trace(TraceGen::ZipfianDistribution(2000, 500, 1, 4));
  TestTrace(&cache, &trace);
  ASSERT_GT(cache.stats().num_hits, 1000);
  ASSERT_LT(cache.stats().num_misses, 1000);
}

TEST(FlexArc, Case1) {
  // Generate 0...20, 0...20, 0..20
  FixedTrace trace(TraceGen::CycleTrace(100, 20, 4));
  trace.Add(TraceGen::CycleTrace(100, 20, 4));
  trace.Add(TraceGen::CycleTrace(100, 20, 4));
  // Add 0...100
  trace.Add(TraceGen::CycleTrace(100, 100, 4));
  // Add 0..20
  trace.Add(TraceGen::CycleTrace(100, 20, 4));

  FlexARC<string, int64_t> cache1(100, 100);
  TestTrace(&cache1, &trace);
  ASSERT_EQ(400, cache1.stats().num_hits);
  ASSERT_EQ(100, cache1.stats().num_misses);

  trace.Reset();
  FlexARC<string, int64_t> cache2(40, 40);
  TestTrace(&cache2, &trace);
  ASSERT_EQ(400, cache2.stats().num_hits);
  ASSERT_EQ(100, cache2.stats().num_misses);

  trace.Reset();
  FlexARC<string, int64_t> cache3(20, 20);
  TestTrace(&cache3, &trace);
  ASSERT_EQ(400, cache3.stats().num_hits);
  ASSERT_EQ(100, cache3.stats().num_misses);

  trace.Reset();
  FlexARC<string, int64_t> cache4(10, 10);
  TestTrace(&cache4, &trace);
  ASSERT_EQ(5, cache4.stats().num_hits);
  ASSERT_EQ(495, cache4.stats().num_misses);
}
