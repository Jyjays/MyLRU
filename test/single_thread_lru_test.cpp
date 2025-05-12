#include "lru_cache.h"
#include <iostream>
#include <chrono>
#include <atomic>
#include <gtest/gtest.h> // Include Google Test framework

// Single thread test: Key<8>, Value<16>, Hash<32>, KeyEqual<32>
// 1. Insert 1000 elements
// 2. Find 1000 elements
// 3. Remove 1000 elements
namespace myLru {
TEST(LRUCacheTest, SingleThreadTest) {
    LRUCache<int, std::string, std::hash<int>, std::equal_to<int>> cache(1000);
    std::string value;
    for (int i = 0; i < 1000; ++i) {
        std::string tempValue = "value" + std::to_string(i);
        EXPECT_TRUE(cache.Insert(i, tempValue));
    }
    for (int i = 0; i < 1000; ++i) {
        EXPECT_TRUE(cache.Find(i, value));
        EXPECT_EQ(value, "value" + std::to_string(i));
    }
    for (int i = 0; i < 1000; ++i) {
        EXPECT_TRUE(cache.Remove(i));
    }
}
} // namespace myLru