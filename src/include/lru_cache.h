#include <atomic>
#include <mutex>

#include "config.h"
#include "hashtable_wrapper.h"

namespace myLru {

// #define OutOfListMarker ((void*)0x1)

#define LRUCACHE_TEMPLATE_ARGUMENTS \
  template <typename Key, typename Value, typename Hash, typename KeyEqual>

#define LRUCACHE LRUCache<Key, Value, Hash, KeyEqual>

#define SEGLRUCACHE SegLRUCache<Key, Value, Hash, KeyEqual>

template <typename Key, typename Value, typename Hash = std::hash<Key>,
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
  LRUCache();
  LRUCache(size_t size);
  LRUCache(const LRUCache&) = delete;
  LRUCache& operator=(const LRUCache&) = delete;
  ~LRUCACHE();
  bool Find(const Key& key, Value& value);

  bool Insert(const Key& key, Value value);

  bool Remove(const Key& key);

  size_t Size();
  void Clear();
  void Resize(size_t size);
  bool IsEmpty() { return cur_size_ == 0; }
  size_t Capacity() { return max_size_; }
  bool IsFull() { return cur_size_ == max_size_; }

 private:
  HashTableWrapper<Key, LRUNode*, Hash, KeyEqual> hash_table_;
  LRUNode* head_;
  LRUNode* tail_;
  std::mutex latch_;
  size_t max_size_;
  std::atomic<size_t> cur_size_;

  void evict();

  void push_node(LRUNode* node);

  void remove_node(LRUNode* node);

  bool remove_helper(const Key& key, LRUNode* del_node);
};

template <typename Key, typename Value, typename Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class SegLRUCache {
 public:
  explicit SegLRUCache(size_t capacity);
  bool Find(const Key& key, Value& value);
  bool Insert(const Key& key, Value value);
  bool Remove(const Key& key);
  size_t Size();
  void Clear();
  void Resize(size_t size);
  size_t Capacity();
  bool IsEmpty();
  bool IsFull();

 private:
  LRUCACHE lru_cache_[segNum];
  static uint32_t Shard(size_t hash) { 
    return static_cast<uint32_t>(hash & (segNum - 1));
  }

  static size_t SegHash(const Key& key) { 
    return std::hash<KeyType>()(key);
  }
};

}  // namespace myLru
