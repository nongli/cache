#include "cache/arc.h"
#include "util/trace-gen.h"
#include "gtest/gtest.h"

using namespace cache;
using namespace std;

TEST(ArcCache, SmallCache) {
  AdaptiveCache<string, string> cache(2);
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

TEST(ArcCache, SmallCacheSized) {
  AdaptiveCache<string, string, NopLock, StringSizer> cache(16);
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

TEST(ArcCache, LRUOnly) {
  AdaptiveCache<string, string> cache(2);
  ASSERT_EQ(cache.size(), 0);
  cache.add_to_cache("Baby Yoda", make_shared<string>("Unknown Name"));
  ASSERT_EQ(cache.size(), 1);
  cache.add_to_cache("The Mandalorian", make_shared<string>("Din Djarin"));
  ASSERT_EQ(cache.size(), 2);
  cache.add_to_cache("Bounty Hunter", make_shared<string>("Boba Fett"));
  ASSERT_EQ(cache.size(), 2);
  ASSERT_EQ(cache.get("Baby Yoda"), nullptr);
}

TEST(ArcCache, Adaptive) {
  AdaptiveCache<string, string> cache(2);
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

void TestTrace(AdaptiveCache<string, string>* cache, Trace* trace) {
  while (true) {
    const Request* r = trace->next();
    if (r == nullptr)
      break;

    shared_ptr<string> val = cache->get(r->key);
    if (!val) {
      cache->add_to_cache(r->key, make_shared<string>(r->value));
    }
  }
}

TEST(ArcCache, SingleKey) {
  AdaptiveCache<string, string> cache(2);
  FixedTrace trace(TraceGen::SameKeyTrace(100, "key", "value"));
  TestTrace(&cache, &trace);
  ASSERT_EQ(99, cache.stats().num_hits);
  ASSERT_EQ(1, cache.stats().num_misses);
}

TEST(ArcCache, AllUniqueKey) {
  AdaptiveCache<string, string> cache(100);
  FixedTrace trace(TraceGen::CycleTrace(100, 100, "value"));
  TestTrace(&cache, &trace);
  ASSERT_EQ(0, cache.stats().num_hits);
  ASSERT_EQ(100, cache.stats().num_misses);
}

TEST(ArcCache, SmallCycle) {
  AdaptiveCache<string, string> cache(100);
  FixedTrace trace(TraceGen::CycleTrace(100, 20, "value"));
  TestTrace(&cache, &trace);
  ASSERT_EQ(80, cache.stats().num_hits);
  ASSERT_EQ(20, cache.stats().num_misses);
}

TEST(ArcCache, BadCycle) {
  // Trace goes 0..10, twice on a cache of 5.
  AdaptiveCache<string, string> cache(5);
  FixedTrace trace(TraceGen::CycleTrace(20, 10, "value"));

  TestTrace(&cache, &trace);
  ASSERT_EQ(1, cache.stats().num_hits);
  ASSERT_EQ(19, cache.stats().num_misses);
  ASSERT_EQ(14, cache.stats().num_evicted);

  trace.Reset();
  TestTrace(&cache, &trace);
  ASSERT_EQ(3, cache.stats().num_hits);
  ASSERT_EQ(37, cache.stats().num_misses);
  ASSERT_EQ(32, cache.stats().num_evicted);
}

TEST(ArcCache, Gaussian) {
  // This will fail with some probability. Retry if this is a problem?
  AdaptiveCache<string, string> cache(100);
  FixedTrace trace(TraceGen::NormalDistribution(500, 20, 5, "value"));
  TestTrace(&cache, &trace);
  ASSERT_GT(cache.stats().num_hits, 400);
  ASSERT_LT(cache.stats().num_misses, 100);

  AdaptiveCache<string, string> cache2(100);
  FixedTrace trace2(TraceGen::NormalDistribution(500, 1000, 100, "value"));
  TestTrace(&cache2, &trace2);
  ASSERT_GT(cache2.stats().num_hits, 50);
  ASSERT_LT(cache2.stats().num_misses, 450);
}

TEST(ArcCache, Poisson) {
  // This will fail with some probability. Retry if this is a problem?
  AdaptiveCache<string, string> cache(100);
  FixedTrace trace(TraceGen::PoissonDistribution(500, 20, "value"));
  TestTrace(&cache, &trace);
  ASSERT_GT(cache.stats().num_hits, 400);
  ASSERT_LT(cache.stats().num_misses, 100);
}

TEST(ArcCache, Zipf) {
  // This will fail with some probability. Retry if this is a problem?
  AdaptiveCache<string, string> cache(100);
  FixedTrace trace(TraceGen::ZipfianDistribution(2000, 500, 1, "value"));
  TestTrace(&cache, &trace);
  ASSERT_GT(cache.stats().num_hits, 1000);
  ASSERT_LT(cache.stats().num_misses, 1000);
}

TEST(ArcCache, Case1) {
  // Generate 0...20, 0...20, 0..20
  FixedTrace trace(TraceGen::CycleTrace(100, 20, "value"));
  trace.Add(TraceGen::CycleTrace(100, 20, "value"));
  trace.Add(TraceGen::CycleTrace(100, 20, "value"));
  // Add 0...100
  trace.Add(TraceGen::CycleTrace(100, 100, "value"));
  // Add 0..20
  trace.Add(TraceGen::CycleTrace(100, 20, "value"));

  AdaptiveCache<string, string> cache1(100);
  TestTrace(&cache1, &trace);
  ASSERT_EQ(400, cache1.stats().num_hits);
  ASSERT_EQ(100, cache1.stats().num_misses);

  trace.Reset();
  AdaptiveCache<string, string> cache2(40);
  TestTrace(&cache2, &trace);
  ASSERT_EQ(400, cache2.stats().num_hits);
  ASSERT_EQ(100, cache2.stats().num_misses);

  trace.Reset();
  AdaptiveCache<string, string> cache3(20);
  TestTrace(&cache3, &trace);
  ASSERT_EQ(399, cache3.stats().num_hits);
  ASSERT_EQ(101, cache3.stats().num_misses);

  trace.Reset();
  AdaptiveCache<string, string> cache4(10);
  TestTrace(&cache4, &trace);
  // You might wonder why 6? This is a problem with ARC. We bump p to 1,
  // which moves 0 to LFU cache when it is accessed a second time. This in
  // turn means that it ends up here.
  ASSERT_EQ(6, cache4.stats().num_hits);
  ASSERT_EQ(494, cache4.stats().num_misses);
}
