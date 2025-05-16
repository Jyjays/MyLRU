#include <gtest/gtest.h>

#include <array>
#include <cstring>
#include <iostream>
#include <numeric>
#include <thread>
#include <vector>

#include "lru_cache.h"

ValueType generateValueForKey(KeyType key) {
  ValueType value{};
  std::memcpy(value.data(), &key, sizeof(KeyType));
  return value;
}

namespace myLru {  // Using your namespace

class SegLRUCacheMultiThreadTest : public ::testing::Test {
 protected:
  // You can add common setup here if needed
};

// --- Multi-threaded Insert and Find Test ---
TEST_F(SegLRUCacheMultiThreadTest, ConcurrentInsertAndFind) {
  const int num_threads = threadNum;
  const int items_per_thread = testsNum / num_threads;
  const size_t capacity_per_segment = testsNum / num_threads;

  SegLRUCache<KeyType, ValueType> cache(capacity_per_segment);
  std::vector<std::thread> threads;

  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&cache, i, items_per_thread]() {
      for (int j = 0; j < items_per_thread; ++j) {
        KeyType key = static_cast<KeyType>(i * items_per_thread + j);
        ValueType value = generateValueForKey(key);
        ASSERT_TRUE(cache.Insert(key, value));
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }
  threads.clear();

  EXPECT_EQ(cache.Size(), static_cast<size_t>(num_threads * items_per_thread));

  // Phase 2: Concurrent Finds
  std::atomic<int> found_count(0);
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&cache, i, items_per_thread, &found_count]() {
      ValueType retrieved_value;
      for (int j = 0; j < items_per_thread; ++j) {
        KeyType key = static_cast<KeyType>(i * items_per_thread + j);
        ValueType expected_value = generateValueForKey(key);
        if (cache.Find(key, retrieved_value)) {
          EXPECT_EQ(retrieved_value, expected_value);
          found_count++;
        }
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }
  cache.GetHis_Miss();
  threads.clear();
  EXPECT_EQ(found_count.load(), num_threads * items_per_thread);
  EXPECT_EQ(
      cache.Size(),
      static_cast<size_t>(num_threads *
                          items_per_thread));  // Size should remain the same
}

// --- Multi-threaded Mixed Operations Test (Insert, Find, Remove) ---
TEST_F(SegLRUCacheMultiThreadTest, ConcurrentMixedOperations) {
  const int num_threads = 8;  // More threads for higher contention
  const int ops_per_thread = testsNum / num_threads;
  const size_t capacity_per_segment = testsNum * size_ratio / num_threads;

  SegLRUCache<KeyType, ValueType> cache(capacity_per_segment);
  std::vector<std::thread> threads;
  std::atomic<int> successful_inserts(0);
  std::atomic<int> successful_finds(0);
  std::atomic<int> successful_removes(0);

  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&, i]() {  // Capture by reference, i by value
      for (int j = 0; j < ops_per_thread; ++j) {
        KeyType key = static_cast<KeyType>(
            (i * ops_per_thread + j) %
            (num_threads * ops_per_thread / 2));  // Keys will overlap

        ValueType value = generateValueForKey(key);
        if (cache.Insert(key, value)) {
          successful_inserts++;
        }
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  threads.clear();

  // Phase 2: Concurrent Finds and Removes
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&, i]() {  // Capture by reference, i by value
      for (int j = 0; j < ops_per_thread; ++j) {
        KeyType key = static_cast<KeyType>(
            (i * ops_per_thread + j) %
            (num_threads * ops_per_thread / 2));  // Keys will overlap

        ValueType retrieved_value;
        if (cache.Find(key, retrieved_value)) {
          successful_finds++;
          if (cache.Remove(key)) {
            successful_removes++;
          }
        }
      }
    });
  }
  for (auto& t : threads) {
    t.join();
  }
  threads.clear();

  std::cout << "Mixed Ops Test - Successful Inserts: "
            << successful_inserts.load() << std::endl;
  std::cout << "Mixed Ops Test - Successful Finds: " << successful_finds.load()
            << std::endl;
  std::cout << "Mixed Ops Test - Successful Removes: "
            << successful_removes.load() << std::endl;
  std::cout << "Mixed Ops Test - Final Cache Size: " << cache.Size()
            << std::endl;

  cache.GetHis_Miss();
  EXPECT_GT(successful_inserts.load(), 0);

}

}  // namespace myLru
