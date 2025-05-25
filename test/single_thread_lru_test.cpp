#include <gtest/gtest.h>

#include <array>
#include <cstring>
#include <iostream>
#include <numeric>
#include <vector>

#include "lru_cache.h"

ValueType generateValueForKey(KeyType key) {
  ValueType value{};
  std::memcpy(value.data(), &key, sizeof(KeyType));
  return value;
}

namespace myLru {  // Using your namespace

// --- Basic Insert, Find, Remove Test ---
TEST(LRUCacheSingleThreadTest, BasicOperations) {
  const size_t capacity = 100;
  LRUCache<KeyType, ValueType> cache(capacity);
  ValueType retrieved_value;

  EXPECT_EQ(cache.Size(), 0);
  EXPECT_TRUE(cache.IsEmpty());
  EXPECT_EQ(cache.Capacity(), capacity);

  // 1. Insert elements
  for (KeyType i = 0; i < static_cast<KeyType>(capacity); ++i) {
    ValueType expected_value = generateValueForKey(i);
    ASSERT_TRUE(cache.Insert(i, expected_value))
        << "Failed to insert key " << i;
    EXPECT_EQ(cache.Size(), static_cast<size_t>(i + 1));
    EXPECT_FALSE(cache.IsEmpty());
  }
  EXPECT_EQ(cache.Size(), capacity);
  EXPECT_TRUE(cache.IsFull());

  for (KeyType i = 0; i < static_cast<KeyType>(capacity); ++i) {
    ValueType expected_value = generateValueForKey(i);
    ASSERT_TRUE(cache.Find(i, retrieved_value)) << "Failed to find key " << i;
    EXPECT_EQ(retrieved_value, expected_value)
        << "Value mismatch for key " << i;
  }

  // 3. Try to find a non-existent key
  EXPECT_FALSE(cache.Find(capacity + 1, retrieved_value));

  // 4. Remove elements
  for (KeyType i = 0; i < static_cast<KeyType>(capacity); ++i) {
    ASSERT_TRUE(cache.Remove(i)) << "Failed to remove key " << i;
    EXPECT_EQ(cache.Size(), capacity - 1 - i);
    EXPECT_FALSE(cache.Find(i, retrieved_value))
        << "Found key " << i << " after removal.";
  }
  EXPECT_EQ(cache.Size(), 0);
  EXPECT_TRUE(cache.IsEmpty());

  // 5. Try removing a non-existent key
  EXPECT_FALSE(cache.Remove(0));
  EXPECT_FALSE(cache.Remove(capacity + 1));
}

// --- Test Cache Eviction Policy (LRU) ---
TEST(LRUCacheSingleThreadTest, EvictionLRU) {
  const size_t capacity = 10;
  LRUCache<KeyType, ValueType> cache(capacity);
  ValueType retrieved_value;

  // 1. Fill the cache completely
  for (KeyType i = 0; i < static_cast<KeyType>(capacity); ++i) {
    ASSERT_TRUE(cache.Insert(i, generateValueForKey(i)));
  }
  EXPECT_EQ(cache.Size(), capacity);
  EXPECT_TRUE(cache.IsFull());

  // 2. Insert one more element - this should evict the least recently used (key
  // 0)
  KeyType new_key = capacity;
  ASSERT_TRUE(cache.Insert(new_key, generateValueForKey(new_key)));
  EXPECT_EQ(cache.Size(), capacity);

  // 3. Verify the evicted element (key 0) is gone
  EXPECT_FALSE(cache.Find(0, retrieved_value))
      << "Key 0 should have been evicted.";

  // 4. Verify the newly added element (key 'capacity') is present
  ASSERT_TRUE(cache.Find(new_key, retrieved_value));
  EXPECT_EQ(retrieved_value, generateValueForKey(new_key));

  // 5. Verify other elements (1 to capacity-1) are still present
  for (KeyType i = 1; i < static_cast<KeyType>(capacity); ++i) {
    ASSERT_TRUE(cache.Find(i, retrieved_value))
        << "Key " << i << " should still be present.";
    EXPECT_EQ(retrieved_value, generateValueForKey(i));
  }
}

// --- Test Updating Existing Key ---
TEST(LRUCacheSingleThreadTest, UpdateValueAndLRUOrder) {
  const size_t capacity = 5;
  LRUCache<KeyType, ValueType> cache(capacity);
  ValueType retrieved_value;

  // 1. Fill the cache (Keys 0, 1, 2, 3, 4)
  for (KeyType i = 0; i < static_cast<KeyType>(capacity); ++i) {
    ASSERT_TRUE(cache.Insert(i, generateValueForKey(i)));
  }

  // 2. Update key 0 - should change its value and make it the most recently
  // used
  KeyType key_to_update = 0;
  ValueType updated_value = generateValueForKey(99);
  ASSERT_TRUE(cache.Insert(key_to_update, updated_value));
  EXPECT_EQ(cache.Size(), capacity);

  // 3. Verify the updated value
  ASSERT_TRUE(cache.Find(key_to_update, retrieved_value));
  EXPECT_EQ(retrieved_value, updated_value);

  // 4. Insert new elements to force eviction (Keys 5, 6)
  ASSERT_TRUE(cache.Insert(5, generateValueForKey(5)));
  ASSERT_TRUE(cache.Insert(6, generateValueForKey(6)));

  EXPECT_EQ(cache.Size(), capacity);

  // 5. Verify evicted keys
  EXPECT_FALSE(cache.Find(1, retrieved_value)) << "Key 1 should be evicted.";
  EXPECT_FALSE(cache.Find(2, retrieved_value)) << "Key 2 should be evicted.";

  // 6. Verify remaining keys are present (0, 3, 4, 5, 6)
  EXPECT_TRUE(cache.Find(0, retrieved_value));
  EXPECT_TRUE(cache.Find(3, retrieved_value));
  EXPECT_TRUE(cache.Find(4, retrieved_value));
  EXPECT_TRUE(cache.Find(5, retrieved_value));
  EXPECT_TRUE(cache.Find(6, retrieved_value));
}

// --- Test LRU Order Change by Access ---
TEST(LRUCacheSingleThreadTest, EvictionAfterAccessOrderChange) {
  const size_t capacity = 5;
  LRUCache<KeyType, ValueType> cache(capacity);
  ValueType retrieved_value;

  // 1. Fill the cache (Keys 0, 1, 2, 3, 4) - Order: 4 (MRU), 3, 2, 1, 0 (LRU)
  for (KeyType i = 0; i < static_cast<KeyType>(capacity); ++i) {
    ASSERT_TRUE(cache.Insert(i, generateValueForKey(i)));
  }

  // 2. Access elements in a specific order to change LRU status
  ASSERT_TRUE(cache.Find(0, retrieved_value));
  ASSERT_TRUE(cache.Find(1, retrieved_value));
  ASSERT_TRUE(cache.Find(2, retrieved_value));

  // 3. Insert a new element (Key 5) - should evict the current LRU (Key 3)
  ASSERT_TRUE(cache.Insert(5, generateValueForKey(5)));
  EXPECT_EQ(cache.Size(), capacity);

  // 4. Verify evicted key
  EXPECT_FALSE(cache.Find(3, retrieved_value)) << "Key 3 should be evicted.";

  // 5. Verify remaining keys are present (0, 1, 2, 4, 5)
  EXPECT_TRUE(cache.Find(0, retrieved_value));
  EXPECT_TRUE(cache.Find(1, retrieved_value));
  EXPECT_TRUE(cache.Find(2, retrieved_value));
  EXPECT_TRUE(cache.Find(4, retrieved_value));
  EXPECT_TRUE(cache.Find(5, retrieved_value));
}

// --- Test Clear Operation ---
TEST(LRUCacheSingleThreadTest, ClearCache) {
  const size_t capacity = 10;
  LRUCache<KeyType, ValueType> cache(capacity);
  ValueType retrieved_value;

  // 1. Insert some elements
  for (KeyType i = 0; i < static_cast<KeyType>(capacity / 2); ++i) {
    ASSERT_TRUE(cache.Insert(i, generateValueForKey(i)));
  }
  EXPECT_EQ(cache.Size(), capacity / 2);

  // 2. Clear the cache
  cache.Clear();
  EXPECT_EQ(cache.Size(), 0);
  EXPECT_TRUE(cache.IsEmpty());
  EXPECT_FALSE(cache.IsFull());

  // 3. Verify elements are gone
  for (KeyType i = 0; i < static_cast<KeyType>(capacity / 2); ++i) {
    EXPECT_FALSE(cache.Find(i, retrieved_value))
        << "Found key " << i << " after Clear().";
  }

  // 4. Test inserting after clear
  ASSERT_TRUE(cache.Insert(100, generateValueForKey(100)));
  EXPECT_EQ(cache.Size(), 1);
  ASSERT_TRUE(cache.Find(100, retrieved_value));
  EXPECT_EQ(retrieved_value, generateValueForKey(100));
}

// --- Test Edge Case: Capacity 1 ---
TEST(LRUCacheSingleThreadTest, CapacityOne) {
  LRUCache<KeyType, ValueType> cache(1);
  ValueType retrieved_value;

  EXPECT_EQ(cache.Capacity(), 1);

  // Insert first element
  ASSERT_TRUE(cache.Insert(1, generateValueForKey(1)));
  EXPECT_EQ(cache.Size(), 1);
  ASSERT_TRUE(cache.Find(1, retrieved_value));
  EXPECT_EQ(retrieved_value, generateValueForKey(1));

  // Insert second element, should evict the first
  ASSERT_TRUE(cache.Insert(2, generateValueForKey(2)));
  EXPECT_EQ(cache.Size(), 1);
  EXPECT_FALSE(cache.Find(1, retrieved_value));
  ASSERT_TRUE(cache.Find(2, retrieved_value));
  EXPECT_EQ(retrieved_value, generateValueForKey(2));

  ASSERT_TRUE(cache.Insert(2, generateValueForKey(99)));
  EXPECT_EQ(cache.Size(), 1);
  ASSERT_TRUE(cache.Find(2, retrieved_value));
  EXPECT_EQ(retrieved_value, generateValueForKey(99));

  // Remove the element
  ASSERT_TRUE(cache.Remove(2));
  EXPECT_EQ(cache.Size(), 0);
  EXPECT_FALSE(cache.Find(2, retrieved_value));
}

}  // namespace myLru
