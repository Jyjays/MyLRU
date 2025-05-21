#pragma once

#include <atomic>
#include <mutex>

#include "config.h"
#include "hash_table_resizer.h"
#include "hashtable_wrapper.h"
namespace myLru {

#define LRUCACHE_TEMPLATE_ARGUMENTS \
  template <typename Key, typename Value, typename Hash, typename KeyEqual>

#define LRUCACHE LRUCache<Key, Value, Hash, KeyEqual>

#define SEGLRUCACHE SegLRUCache<Key, Value, Hash, KeyEqual>

template <typename Key, typename Value, typename Hash = HashFuncImplement,
          typename KeyEqual = std::equal_to<Key>>
class LRUCache {
 public:
  struct LRUNode {
    LRUNode() : next_(nullptr), prev_(nullptr) {}
    LRUNode(const Key& key, const Value& value) : key_(key), value_(value) {}

    LRUNode* next_;
    LRUNode* prev_;
    Key key_;
    Value value_;
  };

  using ResizerType = HashTableResizer<Key, LRUNode*, Hash, KeyEqual>;

  LRUCache();
  LRUCache(size_t size);
  LRUCache(const LRUCache&) = delete;
  LRUCache& operator=(const LRUCache&) = delete;
  ~LRUCACHE();

  auto Find(const Key& key, Value& value) -> bool;

  auto Insert(const Key& key, Value value) -> bool;

  auto Remove(const Key& key) -> bool;

  auto Size() -> size_t;
  auto Clear() -> void;
  auto Resize(size_t size) -> void;
  auto IsEmpty() -> bool { return cur_size_ == 0; }
  auto Capacity() -> size_t { return max_size_; }
  auto IsFull() -> bool { return cur_size_ == max_size_; }

  auto SetResizer(ResizerType* resizer) -> void {
    hash_table_.SetResizer(resizer);
  }

 private:
  HashTableWrapper<Key, LRUNode*, Hash, KeyEqual> hash_table_;
  LRUNode* head_;
  LRUNode* tail_;
  std::mutex latch_;
  size_t max_size_;
  size_t cur_size_;

  auto evict() -> void;

  auto push_node(LRUNode* node) -> void;

  auto remove_node(LRUNode* node) -> void;

  auto remove_helper(const Key& key, LRUNode* del_node) -> bool;
};

template <typename Key, typename Value, typename Hash = HashFuncImplement,
          typename KeyEqual = std::equal_to<Key>>
class SegLRUCache {
 public:
  using ShardType = LRUCache<Key, Value, Hash, KeyEqual>;
  using ResizerForShardsType = typename ShardType::ResizerType;
  explicit SegLRUCache(size_t capacity);
  auto Find(const Key& key, Value& value) -> bool;
  auto Insert(const Key& key, Value value) -> bool;
  auto Remove(const Key& key) -> bool;
  auto Size() -> size_t;
  auto Clear() -> void;
  auto Resize(size_t size) -> void;
  auto Capacity() -> size_t;
  auto IsEmpty() -> bool;
  auto IsFull() -> bool;
  auto GetHis_Miss() -> void;

 private:
  LRUCACHE lru_cache_[segNum];

  std::atomic<size_t> hit_count_ = 0;
  std::atomic<size_t> miss_count_ = 0;

  ResizerForShardsType resizer_;

  static auto Shard(size_t hash) -> uint32_t {
    return static_cast<uint32_t>(hash & (segNum - 1));
  }

  static auto SegHash(const Key& key) -> size_t {
    return Hash()(key);
  }
};

}  // namespace myLru
