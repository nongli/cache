#include "include/tunable-arc.h"
#include "util/trace-gen.h"
#include "gtest/gtest.h"

using namespace cache;
using namespace std;

TEST(TunableArcCache, SmallCache) {
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

TEST(TunableArcCache, LRUOnly) {
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

TEST(TunableArcCache, Adaptive) {
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

void TestTrace(FlexARC<string, string>* cache, Trace* trace) {
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

TEST(TunableArcCache, SingleKey) {
  FlexARC<string, string> cache(2, 2);
  FixedTrace trace(TraceGen::SameKeyTrace(100, "key", "value"));
  TestTrace(&cache, &trace);
  ASSERT_EQ(99, cache.stats().num_hits);
  ASSERT_EQ(1, cache.stats().num_misses);
}

TEST(TunableArcCache, AllUniqueKey) {
  FlexARC<string, string> cache(100, 100);
  FixedTrace trace(TraceGen::CycleTrace(100, 100, "value"));
  TestTrace(&cache, &trace);
  ASSERT_EQ(0, cache.stats().num_hits);
  ASSERT_EQ(100, cache.stats().num_misses);
}

TEST(TunableArcCache, SmallCycle) {
  FlexARC<string, string> cache(100, 100);
  FixedTrace trace(TraceGen::CycleTrace(100, 20, "value"));
  TestTrace(&cache, &trace);
  ASSERT_EQ(80, cache.stats().num_hits);
  ASSERT_EQ(20, cache.stats().num_misses);
}

TEST(TunableArcCache, Gaussian) {
  // This will fail with some probability. Retry if this is a problem?
  FlexARC<string, string> cache(100, 100);
  FixedTrace trace(TraceGen::NormalDistribution(500, 20, 5, "value"));
  TestTrace(&cache, &trace);
  ASSERT_GT(cache.stats().num_hits, 400);
  ASSERT_LT(cache.stats().num_misses, 100);

  FlexARC<string, string> cache2(100, 100);
  FixedTrace trace2(TraceGen::NormalDistribution(500, 1000, 100, "value"));
  TestTrace(&cache2, &trace2);
  ASSERT_GT(cache2.stats().num_hits, 50);
  ASSERT_LT(cache2.stats().num_misses, 450);
}

TEST(TunableArcCache, Poisson) {
  // This will fail with some probability. Retry if this is a problem?
  FlexARC<string, string> cache(100, 100);
  FixedTrace trace(TraceGen::PoissonDistribution(500, 20, "value"));
  TestTrace(&cache, &trace);
  ASSERT_GT(cache.stats().num_hits, 400);
  ASSERT_LT(cache.stats().num_misses, 100);
}

TEST(TunableArcCache, Zipf) {
  // This will fail with some probability. Retry if this is a problem?
  FlexARC<string, string> cache(100, 100);
  FixedTrace trace(TraceGen::ZipfianDistribution(2000, 500, 1, "value"));
  TestTrace(&cache, &trace);
  ASSERT_GT(cache.stats().num_hits, 1000);
  ASSERT_LT(cache.stats().num_misses, 1000);
}

TEST(TunableArcCache, Case1) {
  // Generate 0...20, 0...20, 0..20
  FixedTrace trace(TraceGen::CycleTrace(100, 20, "value"));
  trace.Add(TraceGen::CycleTrace(100, 20, "value"));
  trace.Add(TraceGen::CycleTrace(100, 20, "value"));
  // Add 0...100
  trace.Add(TraceGen::CycleTrace(100, 100, "value"));
  // Add 0..20
  trace.Add(TraceGen::CycleTrace(100, 20, "value"));

  FlexARC<string, string> cache1(100, 100);
  TestTrace(&cache1, &trace);
  ASSERT_EQ(400, cache1.stats().num_hits);
  ASSERT_EQ(100, cache1.stats().num_misses);

  trace.Reset();
  FlexARC<string, string> cache2(40, 40);
  TestTrace(&cache2, &trace);
  ASSERT_EQ(400, cache2.stats().num_hits);
  ASSERT_EQ(100, cache2.stats().num_misses);

  trace.Reset();
  FlexARC<string, string> cache3(20, 20);
  TestTrace(&cache3, &trace);
  ASSERT_EQ(400, cache3.stats().num_hits);
  ASSERT_EQ(100, cache3.stats().num_misses);

  trace.Reset();
  FlexARC<string, string> cache4(10, 10);
  TestTrace(&cache4, &trace);
  ASSERT_EQ(15, cache4.stats().num_hits);
  ASSERT_EQ(485, cache4.stats().num_misses);
}