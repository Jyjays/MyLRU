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

template <typename Key, typename Value, typename Hash = HashFuncImpl,
          typename KeyEqual = std::equal_to<Key>>
class LRUCache {
 public:
  struct LRUNode;
  inline static LRUNode* const OutOfListMarker = reinterpret_cast<LRUNode*>(-1);
  struct LRUNode {
    LRUNode() : next_(nullptr), prev_(nullptr) {}
    LRUNode(const Key& key, const Value& value) : key_(key), value_(value) {}

    LRUNode* next_;
    LRUNode* prev_;
    Key key_;
    Value value_;

    auto inList() -> bool { return prev_ != LRUCache::OutOfListMarker; }
  };

  using ResizerType = HashTableResizer<Key, LRUNode*, Hash, KeyEqual>;

  LRUCache();
  LRUCache(size_t size);
  LRUCache(const LRUCache&) = delete;
  LRUCache& operator=(const LRUCache&) = delete;
  ~LRUCache();

  auto Find(const Key& key, Value& value) -> bool;

  auto Insert(const Key& key, Value value) -> bool;

#ifdef USE_BUFFER
  auto InsertBuffer(LRUNode* buffer_head, LRUNode* buffer_tail) -> void;
#endif
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
#ifdef PRE_ALLOCATE
  std::vector<LRUNode> nodes_;
  std::vector<size_t> free_list_;
#endif

  auto evict() -> void;

  auto push_node(LRUNode* node) -> void;

  auto remove_node(LRUNode* node) -> void;

  auto remove_helper(const Key& key, LRUNode* del_node) -> bool;
#ifdef PRE_ALLOCATE
  auto allocate_node() -> LRUNode*;
  auto release_node(LRUNode* node) -> void;
#endif
};

template <typename Key, typename Value, typename Hash = HashFuncImpl,
          typename KeyEqual = std::equal_to<Key>>
class SegLRUCache {
 public:
  using ShardType = LRUCache<Key, Value>;
  using ResizerForShardsType = typename ShardType::ResizerType;
  using LRUNode = typename ShardType::LRUNode;

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
#ifdef USE_BUFFER
  LRUNode* buffer_[segNum];
  LRUNode* buffer_tail_[segNum];
  size_t buffer_size_[segNum] = {0};
  size_t buffer_capacity_[segNum] = {0};
  std::mutex buffer_latch_[segNum];
#endif

  // std::atomic<size_t> hit_count_ = 0;
  // std::atomic<size_t> miss_count_ = 0;

  ResizerForShardsType resizer_;

  static auto Shard(size_t hash) -> uint32_t {
    return static_cast<uint32_t>(hash & (segNum - 1));
  }

  static auto SegHash(const Key& key) -> size_t { return ShardHashFunc()(key); }
};

}  // namespace myLru
