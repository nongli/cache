#include "gtest/gtest.h"
#include "include/lru.h"

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
