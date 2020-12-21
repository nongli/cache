#include "include/arc.h"
#include "gtest/gtest.h"

TEST(ArcCache, SmallCache) {
  cache::AdaptiveCache<std::string, std::string> cache(2);
  ASSERT_EQ(cache.size(), 0);
  cache.add_to_cache("Baby Yoda",
                     std::make_shared<std::string>("Unknown Name"));
  ASSERT_EQ(cache.size(), 1);
  cache.add_to_cache("Baby Yoda", std::make_shared<std::string>("Grogu"));
  ASSERT_EQ(cache.size(), 1);
  const std::string &val = *cache.get("Baby Yoda");
  ASSERT_TRUE(val == "Grogu");
  cache.add_to_cache("The Mandalorian",
                     std::make_shared<std::string>("Din Djarin"));
  ASSERT_EQ(cache.size(), 2);
  cache.add_to_cache("Bounty Hunter",
                     std::make_shared<std::string>("Boba Fett"));
  ASSERT_EQ(cache.size(), 2);
  ASSERT_EQ(cache.get("The Mandalorian"), nullptr);
}
