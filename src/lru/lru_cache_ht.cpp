#include "lru_cache_ht.h"

#include <vector>
namespace myLru {

// ---------------------------------------
//            LRUCache
//----------------------------------------

LRUCACHEHT_TEMPLATE_ARGUMENTS
LRUCACHEHT::LRUCacheHT() : max_size_(0), cur_size_(0) {
  head_ = new LRUNode();
  tail_ = new LRUNode();
  head_->next_ = tail_;
  tail_->prev_ = head_;
}

LRUCACHEHT_TEMPLATE_ARGUMENTS
LRUCACHEHT::LRUCacheHT(size_t size) : max_size_(size), cur_size_(0) {
  head_ = new LRUNode();
  tail_ = new LRUNode();
  head_->next_ = tail_;
  tail_->prev_ = head_;
}

LRUCACHEHT_TEMPLATE_ARGUMENTS
LRUCACHEHT::~LRUCacheHT() {
  Clear();
  delete head_;
  delete tail_;
}

LRUCACHEHT_TEMPLATE_ARGUMENTS
auto LRUCACHEHT::Find(const Key& key, Value& value) -> bool {
  LRUNode* cur_node;
  if (!hash_table_.Get(key, cur_node)) {
    return false;
  }
  std::unique_lock<std::mutex> lock(latch_, std::try_to_lock);
  if (!lock.owns_lock()) {
    // printf("Failed to acquire lock in Find\n");
    return true;
  }
  // std::unique_lock<std::mutex> lock(latch_);
  // // cur_node 有垂悬指针的可能性
  if (cur_node == nullptr || cur_node->prev_ == nullptr ||
      cur_node->next_ == nullptr) {
    return false;
  }

  value = cur_node->value_;
  remove_node(cur_node);
  push_node(cur_node);
  return true;
}

LRUCACHEHT_TEMPLATE_ARGUMENTS
auto LRUCACHEHT::Insert(const Key& key, Value value) -> bool {
  LRUNode* node = new LRUNode(key, value);
  if (!hash_table_.Insert(key, node)) {
    delete node;
    return false;
  }
  std::unique_lock<std::mutex> lock(latch_);
  if (cur_size_ == max_size_) {
    evict();
  }
  push_node(node);
  cur_size_++;
  return true;
}

LRUCACHEHT_TEMPLATE_ARGUMENTS
auto LRUCACHEHT::Remove(const Key& key) -> bool {
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

LRUCACHEHT_TEMPLATE_ARGUMENTS
auto LRUCACHEHT::Size() -> size_t {
  std::unique_lock<std::mutex> lock(latch_);
  return cur_size_;
}

LRUCACHEHT_TEMPLATE_ARGUMENTS
auto LRUCACHEHT::Clear() -> void {
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

LRUCACHEHT_TEMPLATE_ARGUMENTS
auto LRUCACHEHT::Resize(size_t size) -> void {
  std::unique_lock<std::mutex> lock(latch_);
  if (size < max_size_) {
    while (cur_size_ > size) {
      evict();
    }
  }
  max_size_ = size;
}

LRUCACHEHT_TEMPLATE_ARGUMENTS
auto LRUCACHEHT::evict() -> void {
  LRUNode* last_node = tail_->prev_;
  if (last_node == head_) {
    return;
  }
  remove_node(last_node);
  if (!hash_table_.Remove(last_node->key_)) {
    // LRU_ERR("Failed to remove key from hash table");
  }
  cur_size_--;
  delete last_node;
}

LRUCACHEHT_TEMPLATE_ARGUMENTS
auto LRUCACHEHT::push_node(LRUNode* node) -> void {
  LRUNode* ori_first = head_->next_;
  ori_first->prev_ = node;
  node->next_ = ori_first;
  node->prev_ = head_;
  head_->next_ = node;
}

LRUCACHEHT_TEMPLATE_ARGUMENTS
auto LRUCACHEHT::remove_node(LRUNode* node) -> void {
  LRUNode* ori_next = node->next_;
  LRUNode* ori_prev = node->prev_;
  ori_next->prev_ = ori_prev;
  ori_prev->next_ = ori_next;
  node->next_ = nullptr;
  node->prev_ = nullptr;
}

LRUCACHEHT_TEMPLATE_ARGUMENTS
auto LRUCACHEHT::remove_helper(const Key& key, LRUNode* del_node) -> bool {
  remove_node(del_node);
  hash_table_.Remove(key);
  delete del_node;
  cur_size_--;
  return true;
}

// ---------------------------------------
//            SegLRUCache
//----------------------------------------

LRUCACHEHT_TEMPLATE_ARGUMENTS
SEGLRUCACHEHT::SegLRUCacheHT(size_t capacity) : lru_cache_() {
  for (size_t i = 0; i < segNum; ++i) {
    lru_cache_[i].Resize(capacity);
#ifdef USE_HASH_RESIZER
    lru_cache_[i].SetResizer(&resizer_);
#endif
  }
}

LRUCACHEHT_TEMPLATE_ARGUMENTS
auto SEGLRUCACHEHT::Find(const Key& key, Value& value) -> bool {
  int32_t hash = SegHash(key);
  if (lru_cache_[Shard(hash)].Find(key, value)) {
    hit_count_++;
    return true;
  } else {
    miss_count_++;
    return false;
  }
}

LRUCACHEHT_TEMPLATE_ARGUMENTS
auto SEGLRUCACHEHT::Insert(const Key& key, Value value) -> bool {
  int32_t hash = SegHash(key);
  return lru_cache_[Shard(hash)].Insert(key, value);
}

LRUCACHEHT_TEMPLATE_ARGUMENTS
auto SEGLRUCACHEHT::Remove(const Key& key) -> bool {
  int32_t hash = SegHash(key);
  return lru_cache_[Shard(hash)].Remove(key);
}

LRUCACHEHT_TEMPLATE_ARGUMENTS
auto SEGLRUCACHEHT::Size() -> size_t {
  size_t total_size = 0;
  for (size_t i = 0; i < segNum; ++i) {
    total_size += lru_cache_[i].Size();
  }
  return total_size;
}

LRUCACHEHT_TEMPLATE_ARGUMENTS
auto SEGLRUCACHEHT::Clear() -> void {
  for (size_t i = 0; i < segNum; ++i) {
    lru_cache_[i].Clear();
  }
}

LRUCACHEHT_TEMPLATE_ARGUMENTS
auto SEGLRUCACHEHT::Resize(size_t size) -> void {
  for (size_t i = 0; i < segNum; ++i) {
    lru_cache_[i].Resize(size);
  }
}

LRUCACHEHT_TEMPLATE_ARGUMENTS
auto SEGLRUCACHEHT::Capacity() -> size_t {
  return lru_cache_[0].Capacity() * segNum;
}

LRUCACHEHT_TEMPLATE_ARGUMENTS
auto SEGLRUCACHEHT::IsEmpty() -> bool {
  for (size_t i = 0; i < segNum; ++i) {
    if (!lru_cache_[i].IsEmpty()) {
      return false;
    }
  }
  return true;
}

LRUCACHEHT_TEMPLATE_ARGUMENTS
auto SEGLRUCACHEHT::IsFull() -> bool {
  for (size_t i = 0; i < segNum; ++i) {
    if (!lru_cache_[i].IsFull()) {
      return false;
    }
  }
  return true;
}

LRUCACHEHT_TEMPLATE_ARGUMENTS
auto SEGLRUCACHEHT::GetHis_Miss() -> void {
  printf("Hit Ratio: %.2f%%\n",
         static_cast<double>(hit_count_) / (hit_count_ + miss_count_) * 100);
  printf("Miss Ratio: %.2f%%\n",
         static_cast<double>(miss_count_) / (hit_count_ + miss_count_) * 100);
}

template class LRUCacheHT<KeyType, ValueType, HashType, KeyEqualType>;
template class SegLRUCacheHT<KeyType, ValueType, HashType, KeyEqualType>;

};  // namespace myLru