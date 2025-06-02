#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <shared_mutex>
#include <stdexcept>
#include <utility>
#include <vector>

#include "config.h"
#include "hash_table.h"

namespace myLru {

template <typename Key, typename Value, typename HashFunc = HashFuncImpl,
          typename KeyEqualFunc = std::equal_to<Key>>
class SegHashTable {
 public:
  using HashTableResizerType =
      HashTableResizer<Key, Value, HashFunc, KeyEqualFunc>;

  SegHashTable() : length_(4096), list_(4096), bucket_locks_(4096) {
    // current_list_.store(&list_);
  }

  SegHashTable(size_t size) : length_(size) {
    list_.resize(size);
    bucket_locks_ = std::vector<std::mutex>(size);
    // current_list_.store(&list_);
  }

  bool Insert(const Key& key, Value value_to_insert) {
    size_t bucket_idx = GetBucketIndex(key);
    std::unique_lock<std::mutex> lock(bucket_locks_[bucket_idx]);
    // auto* current_list = current_list_.load();
    auto& chain = list_[bucket_idx];
    for (auto& entry : chain) {
      if (entry.first == key) return false;  // Key exists
    }
    chain.emplace_back(key, value_to_insert);
    // elems_.fetch_add(1);
    // if (elems_.load() > 2 * length_) {
    //   lock.unlock();
    //   Resize();
    // }
    return true;
  }

  bool Get(const Key& key, Value& value_out) {
    size_t bucket_idx = GetBucketIndex(key);
    std::lock_guard<std::mutex> lock(bucket_locks_[bucket_idx]);
    // auto* current_list = current_list_.load();
    auto& chain = list_[bucket_idx];
    for (auto& entry : chain) {
      if (entry.first == key) {
        value_out = entry.second;
        return true;
      }
    }
    return false;
  }

  bool Remove(const Key& key) {
    size_t bucket_idx = GetBucketIndex(key);
    std::lock_guard<std::mutex> lock(bucket_locks_[bucket_idx]);
    // auto* current_list = current_list_.load();
    auto& chain = list_[bucket_idx];
    for (auto it = chain.begin(); it != chain.end(); ++it) {
      if (it->first == key) {
        chain.erase(it);
        // elems_.fetch_sub(1);
        return true;
      }
    }
    return false;
  }

  size_t Size() const { return elems_.load(); }

  void Clear() {
    for (auto& chain : list_) {
      chain.clear();
    }
    elems_.store(0);
  }

  void SetSize(size_t size) {
    length_ = size;
    list_.resize(size);
    // 创建新的mutex vector而不是resize
    bucket_locks_ = std::vector<std::mutex>(size);
  }

  void Resize() {
    // size_t new_length = length_ * 2;
    // std::vector<std::vector<std::pair<Key, Value>>> new_list(new_length);
    // std::vector<std::mutex> new_locks(new_length);

    // auto old_list = list_;
    // for (size_t i = 0; i < length_; ++i) {
    //   std::lock_guard<std::mutex> lock(bucket_locks_[i]);
    //   for (const auto& entry : (*old_list)[i]) {
    //     size_t new_bucket_idx = HashFuncImpl()(entry.first) % new_length;
    //     new_list[new_bucket_idx].emplace_back(entry);
    //   }
    // }

    // list_ = std::move(new_list);
    // bucket_locks_ = std::move(new_locks);
    // current_list_.store(&list_);
    // length_ = new_length;
  }

  void SetResizer(HashTableResizerType* resizer) { resizer_ = resizer; }

 private:
  std::vector<std::vector<std::pair<Key, Value>>> list_;
  // std::atomic<std::vector<std::vector<std::pair<Key, Value>>>*>
  // current_list_;
  std::vector<std::mutex> bucket_locks_;
  size_t length_;
  std::atomic<size_t> elems_{0};
  HashTableResizerType* resizer_ = nullptr;

  size_t GetBucketIndex(const Key& key) {
    return HashFuncImpl()(key) & (length_ - 1);
  }
};

}  // namespace myLru