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
#ifdef PRE_ALLOCATE
  nodes_.reserve(size);
  for (size_t i = 0; i < size; ++i) {
    // nodes : vector<LRUNode> nodes_;
    // nodes_.emplace_back();
    free_list_.push_back(i);
  }
#endif
}

LRUCACHE_TEMPLATE_ARGUMENTS
LRUCACHE::~LRUCache() {
  Clear();
  delete head_;
  delete tail_;
}

LRUCACHE_TEMPLATE_ARGUMENTS
auto LRUCACHE::Find(const Key& key, Value& value) -> bool {
#ifdef USE_HHVM
  LRUNode* cur_node;
  if (!hash_table_.Get(key, cur_node)) {
    return false;
  }
#else
  std::lock_guard<std::mutex> lock(latch_);
  LRUNode* cur_node;
  if (!hash_table_.Get(key, cur_node)) {
    return false;
  }

#endif

  value = cur_node->value_;

#ifdef USE_HHVM
  std::unique_lock<std::mutex> lock(latch_, std::try_to_lock);

  if (!lock.owns_lock()) {
    value = cur_node->value_;
    return true;
  }
#endif

#ifdef USE_HASH_RESIZER
  if (cur_node == nullptr || cur_node->key_ != key ||
      cur_node->next_ == nullptr || cur_node->prev_ == nullptr) {
    LRU_ERR("Something wrong in hashtable.");
    std::cout << key << " " << cur_node->key_ << std::endl;
    return false;
  }
#endif

  if (cur_node->inList()) {
    remove_node(cur_node);
    push_node(cur_node);
  }
  return true;
}

LRUCACHE_TEMPLATE_ARGUMENTS
auto LRUCACHE::Insert(const Key& key, Value value) -> bool {
  std::lock_guard<std::mutex> lock(latch_);
  if (cur_size_ == max_size_) {
    evict();
  }
#ifdef PRE_ALLOCATE
  LRUNode* new_node = allocate_node();
  if (new_node == nullptr) {
    return false;  // No free nodes available
  }
  new_node->key_ = key;
  new_node->value_ = value;

  if (!hash_table_.Insert(key, new_node)) {
    release_node(new_node);
    return false;
  }

  push_node(new_node);
  cur_size_++;
  return true;
#else

  LRUNode* new_node = new LRUNode(key, value);
  if (!hash_table_.Insert(key, new_node)) {
    delete new_node;
    return false;
  }

  push_node(new_node);
  cur_size_++;
  return true;
#endif
}

LRUCACHE_TEMPLATE_ARGUMENTS
auto LRUCACHE::Remove(const Key& key) -> bool {
  std::lock_guard<std::mutex> lock(latch_);
  if (cur_size_ == 0) {
    return false;
  }
#ifdef PRE_ALLOCATE
  LRUNode* to_remove;
  if (!hash_table_.Get(key, to_remove)) {
    return false;  // Key not found
  }
  if (to_remove == nullptr || to_remove->key_ != key ||
      to_remove->next_ == nullptr || to_remove->prev_ == nullptr) {
    LRU_ERR("Something wrong in hashtable.");
    return false;
  }
  return remove_helper(key, to_remove);
#else
  LRUNode* cur_node;
  if (!hash_table_.Get(key, cur_node)) {
    return false;
  }
  return remove_helper(key, cur_node);
#endif
}

LRUCACHE_TEMPLATE_ARGUMENTS
auto LRUCACHE::Size() -> size_t {
  std::lock_guard<std::mutex> lock(latch_);
  return cur_size_;
}

LRUCACHE_TEMPLATE_ARGUMENTS
auto LRUCACHE::Clear() -> void {
  std::lock_guard<std::mutex> lock(latch_);
#ifdef PRE_ALLOCATE
  for (size_t i = 0; i < nodes_.size(); ++i) {
    nodes_[i].next_ = nullptr;
    nodes_[i].prev_ = nullptr;
  }
  free_list_.clear();
  free_list_.reserve(nodes_.size());
  for (size_t i = 0; i < nodes_.size(); ++i) {
    free_list_.push_back(i);
  }
#else
  LRUNode* cur_node = head_->next_;
  while (cur_node != tail_) {
    LRUNode* next_node = cur_node->next_;
    delete cur_node;
    cur_node = next_node;
  }
#endif
  head_->next_ = tail_;
  tail_->prev_ = head_;
  hash_table_.Clear();
  cur_size_ = 0;
}

LRUCACHE_TEMPLATE_ARGUMENTS
auto LRUCACHE::Resize(size_t size) -> void {
  std::lock_guard<std::mutex> lock(latch_);
  if (size < max_size_) {
    while (cur_size_ > size) {
      evict();
    }
  }
  max_size_ = size;
#ifdef PRE_ALLOCATE
  nodes_.resize(size);
  free_list_.clear();
  free_list_.reserve(size);
  for (size_t i = 0; i < size; ++i) {
    free_list_.push_back(i);
  }
#endif
}

LRUCACHE_TEMPLATE_ARGUMENTS
auto LRUCACHE::evict() -> void {
  LRUNode* last_node = tail_->prev_;
  if (last_node == head_) {
    return;
  }
#ifdef PRE_ALLOCATE
  remove_node(last_node);
  release_node(last_node);
  if (!hash_table_.Remove(last_node->key_)) {
    // LRU_ERR("Failed to remove key from hash table");
  }
  cur_size_--;
#else
  remove_node(last_node);
  if (!hash_table_.Remove(last_node->key_)) {
    // LRU_ERR("Failed to remove key from hash table");
  }
  cur_size_--;
  delete last_node;
#endif
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
#ifdef PRE_ALLOCATE
  release_node(del_node);
#else
  delete del_node;
#endif
  cur_size_--;
  return true;
}
#ifdef USE_BUFFER
LRUCACHE_TEMPLATE_ARGUMENTS
auto LRUCACHE::InsertBuffer(LRUNode* buffer_head, LRUNode* buffer_tail)
    -> void {
  std::lock_guard<std::mutex> lock(latch_);
  head_ = buffer_head;
  tail_ = buffer_tail;
  LRUNode* cur_node = head_->next_;
  while (cur_node != tail_) {
    LRUNode* next_node = cur_node->next_;
    if (!hash_table_.Insert(cur_node->key_, cur_node)) {
      // If insertion fails, we need to remove the node from the list
      remove_node(cur_node);
      delete cur_node;
    } else {
      cur_size_++;
    }
    cur_node = next_node;
  }
}
#endif

#ifdef PRE_ALLOCATE
LRUCACHE_TEMPLATE_ARGUMENTS
auto LRUCACHE::allocate_node() -> LRUNode* {
  if (free_list_.empty()) {
    return nullptr;  // No free nodes available
  }
  size_t index = free_list_.back();
  free_list_.pop_back();
  return &nodes_[index];
}

LRUCACHE_TEMPLATE_ARGUMENTS
auto LRUCACHE::release_node(LRUNode* node) -> void {
  if (node == nullptr) {
    return;
  }
  size_t index = node - &nodes_[0];
  free_list_.push_back(index);
  node->next_ = nullptr;
  node->prev_ = nullptr;
}
#endif

// ---------------------------------------
//            SegLRUCache
//----------------------------------------

LRUCACHE_TEMPLATE_ARGUMENTS
SEGLRUCACHE::SegLRUCache(size_t capacity_per_seg) : lru_cache_() {
  for (size_t i = 0; i < segNum; ++i) {
    lru_cache_[i].Resize(capacity_per_seg);
#ifdef USE_HASH_RESIZER
    lru_cache_[i].SetResizer(&resizer_);
#endif

#ifdef USE_BUFFER
    buffer_[i] = new LRUNode();
    buffer_tail_[i] = new LRUNode();
    buffer_[i]->next_ = buffer_tail_[i];
    buffer_[i]->prev_ = nullptr;

    buffer_tail_[i]->prev_ = buffer_[i];
    buffer_tail_[i]->next_ = nullptr;
    buffer_size_[i] = 0;
    buffer_capacity_[i] = capacity_per_seg;
#endif
  }
}

LRUCACHE_TEMPLATE_ARGUMENTS
auto SEGLRUCACHE::Find(const Key& key, Value& value) -> bool {
  int32_t hash = SegHash(key);
  if (lru_cache_[Shard(hash)].Find(key, value)) {
    // hit_count_++;
    return true;
  }
  return false;
}

LRUCACHE_TEMPLATE_ARGUMENTS
auto SEGLRUCACHE::Insert(const Key& key, Value value) -> bool {
  uint32_t shard_idx = Shard(SegHash(key));
#ifdef USE_BUFFER
  std::unique_lock<std::mutex> lock(buffer_latch_[shard_idx]);
  if (buffer_size_[shard_idx] >= buffer_capacity_[shard_idx]) {
    // Buffer is full, flush it to the LRU cache
    lru_cache_[shard_idx].InsertBuffer(buffer_[shard_idx],
                                       buffer_tail_[shard_idx]);
    buffer_[shard_idx]->next_ = buffer_tail_[shard_idx];
    buffer_tail_[shard_idx]->prev_ = buffer_[shard_idx];
    buffer_size_[shard_idx] = 0;  // Reset buffer size after flushing
  }
  // Insert into the buffer
  LRUNode* new_node = new LRUNode(key, value);
  LRUNode* ori_first = buffer_[shard_idx]->next_;
  ori_first->prev_ = new_node;
  new_node->next_ = ori_first;
  new_node->prev_ = buffer_[shard_idx];
  buffer_[shard_idx]->next_ = new_node;
  buffer_size_[shard_idx]++;
  return true;
#else
  return lru_cache_[shard_idx].Insert(key, value);
#endif
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

LRUCACHE_TEMPLATE_ARGUMENTS
auto SEGLRUCACHE::GetHis_Miss() -> void {
  // printf("Hit Ratio: %.2f%%\n",
  //        static_cast<double>(hit_count_) / (hit_count_ + miss_count_) * 100);
  // printf("Miss Ratio: %.2f%%\n",
  //        static_cast<double>(miss_count_) / (hit_count_ + miss_count_) *
  //        100);
}

template class LRUCache<KeyType, ValueType, HashType, KeyEqualType>;
template class SegLRUCache<KeyType, ValueType, HashType, KeyEqualType>;

};  // namespace myLru