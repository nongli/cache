#include "include/lru.h"
#include "gtest/gtest.h"

TEST(LRUTest, ListOfSizeOne) {
  // Create a list
  cache::LRUList<int, int> list;
  cache::LRULink<int, int> l0(0, nullptr);
  // Check initial invariants
  ASSERT_EQ(list.size(), 0);
  ASSERT_EQ(list.peek_head(), nullptr);
  ASSERT_EQ(list.peek_tail(), nullptr);
  // Insert
  list.insert_head(&l0);
  ASSERT_EQ(list.size(), 1);
  ASSERT_EQ(list.peek_head(), &l0);
  ASSERT_EQ(list.peek_tail(), &l0);
  // Move to head
  list.move_to_head(&l0);
  ASSERT_EQ(list.size(), 1);
  ASSERT_EQ(list.peek_head(), &l0);
  ASSERT_EQ(list.peek_tail(), &l0);
}

TEST(LRUTest, ListOfSizeTwo) {
  // Create a list
  cache::LRUList<int, int> list;
  cache::LRULink<int, int> l0(0, nullptr);
  cache::LRULink<int, int> l1(1, nullptr);
  // Insert
  list.insert_head(&l0);
  list.insert_head(&l1);
  ASSERT_EQ(list.size(), 2);
  ASSERT_EQ(list.peek_head(), &l1);
  ASSERT_EQ(list.peek_tail(), &l0);
  // Move to head
  list.move_to_head(&l1);
  ASSERT_EQ(list.size(), 2);
  ASSERT_EQ(list.peek_head(), &l1);
  ASSERT_EQ(list.peek_tail(), &l0);
}

TEST(LRUTest, ListOfSizeThree) {
  // Create a list
  cache::LRUList<int, int> list;
  cache::LRULink<int, int> l0(0, nullptr);
  cache::LRULink<int, int> l1(1, nullptr);
  cache::LRULink<int, int> l2(2, nullptr);
  // Insert
  list.insert_head(&l0);
  list.insert_head(&l1);
  list.insert_head(&l2);
  ASSERT_EQ(list.size(), 3);
  ASSERT_EQ(list.peek_head(), &l2);
  ASSERT_EQ(list.peek_tail(), &l0);
  // Move to head
  list.move_to_head(&l1);
  ASSERT_EQ(list.size(), 3);
  ASSERT_EQ(list.peek_head(), &l1);
  ASSERT_EQ(list.peek_tail(), &l0);
}

TEST(LRUCache, SmallCache) {
  cache::LRUCache<std::string, std::string> cache(2);
  ASSERT_EQ(cache.current_size(), 0);
  size_t evicted = cache.add_to_cache(
      "Baby Yoda", std::make_shared<std::string>("Unknown Name"));
  ASSERT_EQ(evicted, 0);
  ASSERT_EQ(cache.current_size(), 1);
  evicted =
      cache.add_to_cache("Baby Yoda", std::make_shared<std::string>("Grogu"));
  ASSERT_EQ(evicted, 0);
  ASSERT_EQ(cache.current_size(), 1);
  auto val = *cache.get_element("Baby Yoda");
  ASSERT_TRUE(val == "Grogu");
  evicted = cache.add_to_cache("The Mandalorian",
                               std::make_shared<std::string>("Din Djarin"));
  ASSERT_EQ(evicted, 0);
  ASSERT_EQ(cache.current_size(), 2);
  evicted = cache.add_to_cache("Bounty Hunter",
                               std::make_shared<std::string>("Boba Fett"));
  ASSERT_EQ(evicted, 1);
  ASSERT_EQ(cache.current_size(), 2);
}
