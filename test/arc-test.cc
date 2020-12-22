#include "include/arc.h"
#include "util/trace-gen.h"
#include "gtest/gtest.h"

using namespace cache;
using namespace std;

TEST(ArcCache, SmallCache) {
  AdaptiveCache<string, string> cache(2);
  ASSERT_EQ(cache.size(), 0);
  cache.add_to_cache("Baby Yoda",
                     make_shared<string>("Unknown Name"));
  ASSERT_EQ(cache.size(), 1);
  cache.add_to_cache("Baby Yoda", make_shared<string>("Grogu"));
  ASSERT_EQ(cache.size(), 1);
  const string &val = *cache.get("Baby Yoda");
  ASSERT_TRUE(val == "Grogu");
  cache.add_to_cache("The Mandalorian",
                     make_shared<string>("Din Djarin"));
  ASSERT_EQ(cache.size(), 2);
  cache.add_to_cache("Bounty Hunter",
                     make_shared<string>("Boba Fett"));
  ASSERT_EQ(cache.size(), 2);
  ASSERT_EQ(cache.get("The Mandalorian"), nullptr);

  shared_ptr<string> p = cache.remove_from_cache("Baby Yoda");
  ASSERT_EQ(p.use_count(), 1);
  ASSERT_TRUE(*p == "Grogu");
  ASSERT_EQ(cache.size(), 1);
  ASSERT_EQ(cache.get("Baby Yoda"), nullptr);
}

TEST(ArcCache, LRUOnly) {
  AdaptiveCache<string, string> cache(2);
  ASSERT_EQ(cache.size(), 0);
  cache.add_to_cache("Baby Yoda",
                     make_shared<string>("Unknown Name"));
  ASSERT_EQ(cache.size(), 1);
  cache.add_to_cache("The Mandalorian",
                     make_shared<string>("Din Djarin"));
  ASSERT_EQ(cache.size(), 2);
  cache.add_to_cache("Bounty Hunter",
                     make_shared<string>("Boba Fett"));
  ASSERT_EQ(cache.size(), 2);
  ASSERT_EQ(cache.get("Baby Yoda"), nullptr);
}

TEST(ArcCache, Adaptive) {
  AdaptiveCache<string, string> cache(2);
  ASSERT_EQ(cache.size(), 0);
  cache.add_to_cache("Baby Yoda",
                     make_shared<string>("Unknown Name"));
  ASSERT_EQ(cache.size(), 1);
  // Push to LFU side
  const string &val = *cache.get("Baby Yoda");
  ASSERT_TRUE(val == "Unknown Name");
  // Adds to LRU
  cache.add_to_cache("The Mandalorian",
                     make_shared<string>("Din Djarin"));
  ASSERT_EQ(cache.size(), 2);
  // Adds to LRU
  cache.add_to_cache("Bounty Hunter",
                     make_shared<string>("Boba Fett"));
  ASSERT_EQ(cache.size(), 2);
  // Trigger adaptation
  cache.add_to_cache("The Mandalorian",
                     make_shared<string>("Din Djarin"));
  ASSERT_EQ(cache.size(), 2);
  ASSERT_EQ(cache.get("Baby Yoda"), nullptr);
}

void TestTrace(AdaptiveCache<string, string>* cache, Trace* trace) {
  while (true) {
    const Request* r = trace->next();
    if (r == nullptr) break;

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
  ASSERT_EQ(98, cache.stats().num_hits);
  ASSERT_EQ(2, cache.stats().num_misses);
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
  ASSERT_EQ(60, cache.stats().num_hits);
  ASSERT_EQ(40, cache.stats().num_misses);
}


