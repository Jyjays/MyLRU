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
bool LRUCACHE::Find(const Key& key, Value& value) {
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
bool LRUCACHE::Insert(const Key& key, Value value) {
  LRUNode* node;
  std::unique_lock<std::mutex> lock(latch_);
  if (hash_table_.Get(key, node)) {
    // If the key already exists, update the value and move it to the head
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
bool LRUCACHE::Remove(const Key& key) {
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
size_t LRUCACHE::Size() {
  std::unique_lock<std::mutex> lock(latch_);
  return cur_size_;
}

LRUCACHE_TEMPLATE_ARGUMENTS
void LRUCACHE::Clear() {
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
void LRUCACHE::Resize(size_t size) {
  std::unique_lock<std::mutex> lock(latch_);
  if (size < max_size_) {
    while (cur_size_ > size) {
      evict();
    }
  }
  max_size_ = size;
}

LRUCACHE_TEMPLATE_ARGUMENTS
void LRUCACHE::evict() {
  // Single thread version
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
void LRUCACHE::push_node(LRUNode* node) {
  LRU_ASSERT(node != head_ && node != tail_ && node != nullptr, "Invalid node");

  LRUNode* ori_first = head_->next_;
  ori_first->prev_ = node;
  node->next_ = ori_first;
  node->prev_ = head_;
  head_->next_ = node;
}

LRUCACHE_TEMPLATE_ARGUMENTS
void LRUCACHE::remove_node(LRUNode* node) {
  assert(node != head_ && node != tail_ && node != nullptr);
  LRUNode* ori_next = node->next_;
  LRUNode* ori_prev = node->prev_;
  ori_next->prev_ = ori_prev;
  ori_prev->next_ = ori_next;
  node->next_ = nullptr;
  node->prev_ = nullptr;
}

LRUCACHE_TEMPLATE_ARGUMENTS
bool LRUCACHE::remove_helper(const Key& key, LRUNode* del_node) {
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
bool SEGLRUCACHE::Find(const Key& key, Value& value) {
  int32_t hash = SegHash(key);
  return lru_cache_[Shard(hash)].Find(key, value);
}

LRUCACHE_TEMPLATE_ARGUMENTS
bool SEGLRUCACHE::Insert(const Key& key, Value value) {
  int32_t hash = SegHash(key);
  return lru_cache_[Shard(hash)].Insert(key, value);
}

LRUCACHE_TEMPLATE_ARGUMENTS
bool SEGLRUCACHE::Remove(const Key& key) {
  int32_t hash = SegHash(key);
  return lru_cache_[Shard(hash)].Remove(key);
}

LRUCACHE_TEMPLATE_ARGUMENTS
size_t SEGLRUCACHE::Size() {
  size_t total_size = 0;
  for (size_t i = 0; i < segNum; ++i) {
    total_size += lru_cache_[i].Size();
  }
  return total_size;
}

LRUCACHE_TEMPLATE_ARGUMENTS
void SEGLRUCACHE::Clear() {
  for (size_t i = 0; i < segNum; ++i) {
    lru_cache_[i].Clear();
  }
}

LRUCACHE_TEMPLATE_ARGUMENTS
void SEGLRUCACHE::Resize(size_t size) {
  for (size_t i = 0; i < segNum; ++i) {
    lru_cache_[i].Resize(size);
  }
}

LRUCACHE_TEMPLATE_ARGUMENTS
size_t SEGLRUCACHE::Capacity() {
  return lru_cache_[0].Capacity() * segNum;
}

LRUCACHE_TEMPLATE_ARGUMENTS
bool SEGLRUCACHE::IsEmpty() {
  for (size_t i = 0; i < segNum; ++i) {
    if (!lru_cache_[i].IsEmpty()) {
      return false;
    }
  }
  return true;
}

LRUCACHE_TEMPLATE_ARGUMENTS
bool SEGLRUCACHE::IsFull() {
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