#include "lru_cache.h"

#include <vector>
namespace myLru {

// ---------------------------------------
//            LRUCache
//----------------------------------------

LRUCACHE_TEMPLATE_ARGUMENTS
LRUCACHE::LRUCache() : max_size_(0), cur_size_(0) {
  head_ = new LRUNode();
  tail_ = new LRUNode();
  head_->next_ = tail_;
  tail_->prev_ = head_;
}

LRUCACHE_TEMPLATE_ARGUMENTS
LRUCACHE::LRUCache(size_t size) : max_size_(size), cur_size_(0) {
  head_ = new LRUNode();
  tail_ = new LRUNode();
  head_->next_ = tail_;
  tail_->prev_ = head_;
}

LRUCACHE_TEMPLATE_ARGUMENTS
LRUCACHE::~LRUCACHE() {
  Clear();
  delete head_;
  delete tail_;
}

LRUCACHE_TEMPLATE_ARGUMENTS
auto LRUCACHE::Find(const Key& key, Value& value) -> bool {
  std::unique_lock<std::mutex> lock(latch_);
  LRUNode* cur_node;
  if (!hash_table_.Get(key, cur_node)) {
    return false;
  }
  value = cur_node->value_;
  remove_node(cur_node);
  push_node(cur_node);
  return true;
}

LRUCACHE_TEMPLATE_ARGUMENTS
auto LRUCACHE::Insert(const Key& key, Value value) -> bool {
  LRUNode* node;
  std::unique_lock<std::mutex> lock(latch_);
  if (hash_table_.Get(key, node)) {
    LRUNode* new_node = new LRUNode(key, value);
    remove_helper(key, node);
    push_node(new_node);
    if (!hash_table_.Insert(key, new_node)) {
      delete new_node;
      return false;
    }
    cur_size_++;
    return true;
  }
  LRUNode* new_node = new LRUNode(key, value);
  if (!hash_table_.Insert(key, new_node)) {
    delete new_node;
    return false;
  }
  cur_size_++;
  push_node(new_node);
  if (cur_size_ > max_size_) {
    evict();
  }
  return true;
}

LRUCACHE_TEMPLATE_ARGUMENTS
auto LRUCACHE::Remove(const Key& key) -> bool {
  std::unique_lock<std::mutex> lock(latch_);
  if (cur_size_ == 0) {
    return false;
  }
  LRUNode* cur_node;
  if (!hash_table_.Get(key, cur_node)) {
    return false;
  }
  return remove_helper(key, cur_node);
}

LRUCACHE_TEMPLATE_ARGUMENTS
auto LRUCACHE::Size() -> size_t {
  std::unique_lock<std::mutex> lock(latch_);
  return cur_size_;
}

LRUCACHE_TEMPLATE_ARGUMENTS
auto LRUCACHE::Clear() -> void {
  std::unique_lock<std::mutex> lock(latch_);
  LRUNode* cur_node = head_->next_;
  while (cur_node != tail_) {
    LRUNode* next_node = cur_node->next_;
    delete cur_node;
    cur_node = next_node;
  }
  head_->next_ = tail_;
  tail_->prev_ = head_;
  hash_table_.Clear();
  cur_size_ = 0;
}

LRUCACHE_TEMPLATE_ARGUMENTS
auto LRUCACHE::Resize(size_t size) -> void {
  std::unique_lock<std::mutex> lock(latch_);
  if (size < max_size_) {
    while (cur_size_ > size) {
      evict();
    }
  }
  max_size_ = size;
}

LRUCACHE_TEMPLATE_ARGUMENTS
auto LRUCACHE::evict() -> void {
  LRUNode* last_node = tail_->prev_;
  if (last_node == head_) {
    return;
  }
  remove_node(last_node);
  if (!hash_table_.Remove(last_node->key_)) {
    LRU_ERR("Failed to remove key from hash table");
  }
  cur_size_--;
  delete last_node;
}

LRUCACHE_TEMPLATE_ARGUMENTS
auto LRUCACHE::push_node(LRUNode* node) -> void {
  LRUNode* ori_first = head_->next_;
  ori_first->prev_ = node;
  node->next_ = ori_first;
  node->prev_ = head_;
  head_->next_ = node;
}

LRUCACHE_TEMPLATE_ARGUMENTS
auto LRUCACHE::remove_node(LRUNode* node) -> void {
  LRUNode* ori_next = node->next_;
  LRUNode* ori_prev = node->prev_;
  ori_next->prev_ = ori_prev;
  ori_prev->next_ = ori_next;
  node->next_ = nullptr;
  node->prev_ = nullptr;
}

LRUCACHE_TEMPLATE_ARGUMENTS
auto LRUCACHE::remove_helper(const Key& key, LRUNode* del_node) -> bool {
  remove_node(del_node);
  hash_table_.Remove(key);
  delete del_node;
  cur_size_--;
  return true;
}

// ---------------------------------------
//            SegLRUCache
//----------------------------------------

LRUCACHE_TEMPLATE_ARGUMENTS
SEGLRUCACHE::SegLRUCache(size_t capacity) : lru_cache_() {
  for (size_t i = 0; i < segNum; ++i) {
    lru_cache_[i].Resize(capacity);
  }
}

LRUCACHE_TEMPLATE_ARGUMENTS
auto SEGLRUCACHE::Find(const Key& key, Value& value) -> bool {
  int32_t hash = SegHash(key);
  return lru_cache_[Shard(hash)].Find(key, value);
}

LRUCACHE_TEMPLATE_ARGUMENTS
auto SEGLRUCACHE::Insert(const Key& key, Value value) -> bool {
  int32_t hash = SegHash(key);
  return lru_cache_[Shard(hash)].Insert(key, value);
}

LRUCACHE_TEMPLATE_ARGUMENTS
auto SEGLRUCACHE::Remove(const Key& key) -> bool {
  int32_t hash = SegHash(key);
  return lru_cache_[Shard(hash)].Remove(key);
}

LRUCACHE_TEMPLATE_ARGUMENTS
auto SEGLRUCACHE::Size() -> size_t {
  size_t total_size = 0;
  for (size_t i = 0; i < segNum; ++i) {
    total_size += lru_cache_[i].Size();
  }
  return total_size;
}

LRUCACHE_TEMPLATE_ARGUMENTS
auto SEGLRUCACHE::Clear() -> void {
  for (size_t i = 0; i < segNum; ++i) {
    lru_cache_[i].Clear();
  }
}

LRUCACHE_TEMPLATE_ARGUMENTS
auto SEGLRUCACHE::Resize(size_t size) -> void {
  for (size_t i = 0; i < segNum; ++i) {
    lru_cache_[i].Resize(size);
  }
}

LRUCACHE_TEMPLATE_ARGUMENTS
auto SEGLRUCACHE::Capacity() -> size_t {
  return lru_cache_[0].Capacity() * segNum;
}

LRUCACHE_TEMPLATE_ARGUMENTS
auto SEGLRUCACHE::IsEmpty() -> bool {
  for (size_t i = 0; i < segNum; ++i) {
    if (!lru_cache_[i].IsEmpty()) {
      return false;
    }
  }
  return true;
}

LRUCACHE_TEMPLATE_ARGUMENTS
auto SEGLRUCACHE::IsFull() -> bool {
  for (size_t i = 0; i < segNum; ++i) {
    if (!lru_cache_[i].IsFull()) {
      return false;
    }
  }
  return true;
}

template class LRUCache<KeyType, ValueType, HashType, KeyEqualType>;
template class SegLRUCache<KeyType, ValueType, HashType, KeyEqualType>;

};  // namespace myLru