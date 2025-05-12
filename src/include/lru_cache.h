#include <atomic>
#include <mutex>

#include "hashtable_wrapper.h"

namespace myLru {

// #define OutOfListMarker ((void*)0x1)

#define LRUCACHE_TEMPLATE_ARGUMENTS \
  template <typename Key, typename Value, typename Hash, typename KeyEqual>

#define LRUCACHE LRUCache<Key, Value, Hash, KeyEqual>

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
  explicit LRUCache(size_t size);

  bool Find(const Key& key, Value& value);

  bool Insert(const Key& key, Value& value);

  bool Remove(const Key& key);

  size_t Size();
  void Clear();
  void Resize(size_t size);

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
};
}  // namespace myLru
