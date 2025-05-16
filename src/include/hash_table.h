#pragma once

#include <condition_variable>
#include <functional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace myLru {

template <typename K, typename V, typename HF = std::hash<K>,
          typename KEF = std::equal_to<K>>
class HashTableResizer;

template <typename Key, typename Value, typename HashFunc = std::hash<Key>,
          typename KeyEqualFunc = std::equal_to<Key>>
class MyHashTable {
 public:
  using HashTableResizerType =
      HashTableResizer<Key, Value, HashFunc, KeyEqualFunc>;

  explicit MyHashTable(size_t initial_buckets = 16) : elems_(0) {
    if (initial_buckets == 0) {
      initial_buckets = 1;
    }

    length_ = 1;
    while (length_ < initial_buckets) {
      length_ <<= 1;
    }
    list_.resize(length_);
  }

  auto Get(const Key& key, Value& value_out) -> bool {
    Value* found_value_ptr = FindValuePtr(key);
    if (found_value_ptr != nullptr) {
      value_out = *found_value_ptr;
      return true;
    }
    return false;
  }

  auto Insert(const Key& key, Value value_to_insert) -> bool {
    size_t bucket_idx = GetBucketIndex(key);
    std::vector<std::pair<Key, Value>>& chain = list_[bucket_idx];

    for (auto& pair_entry : chain) {
      if (key_equal_(pair_entry.first, key)) {
        pair_entry.second = value_to_insert;
        return true;
      }
    }
    if (resizing_) {
      // If resizing, check in the temp list
      size_t temp_bucket_idx = GetBucketIndexInternal(key, temp_list_.size());
      std::vector<std::pair<Key, Value>>& temp_chain =
          temp_list_[temp_bucket_idx];
      for (auto& pair_entry : temp_chain) {
        if (key_equal_(pair_entry.first, key)) {
          pair_entry.second = value_to_insert;
          return true;
        }
      }
      // If not found, insert into the temp list.
      std::lock_guard<std::mutex> lock(latch_);
      temp_chain.emplace_back(key, value_to_insert);
      return true;
    } else {
      chain.emplace_back(key, value_to_insert);
    }

    elems_++;
    if (elems_ > length_) {
      if (resizer_ != nullptr) {
        resizing_ = true;
        resizer_->EnqueueResize(this);
      } else {
        Resize();
      }
    }
    return true;
  }

  auto Remove(const Key& key) -> bool {
    size_t bucket_idx = GetBucketIndex(key);
    std::vector<std::pair<Key, Value>>& chain = list_[bucket_idx];
    if (resizing_) {
      std::lock_guard<std::mutex> lock(latch_);
      size_t temp_bucket_idx = GetBucketIndexInternal(key, temp_list_.size());
      std::vector<std::pair<Key, Value>>& temp_chain =
          temp_list_[temp_bucket_idx];
      for (auto iter = temp_chain.begin(); iter != temp_chain.end(); ++iter) {
        if (key_equal_(iter->first, key)) {
          temp_chain.erase(iter);
        }
      }
    }
    for (auto iter = chain.begin(); iter != chain.end(); ++iter) {
      if (key_equal_(iter->first, key)) {
        chain.erase(iter);  // 从链中移除
        elems_--;
        return true;
      }
    }
    return false;
  }

  auto Resize() -> void {
    size_t old_length = length_;
    length_ <<= 1;

    // Avoid overflow
    if (length_ == 0) {
      length_ = old_length > 0 ? old_length * 2 : 16;
      if (length_ == 0) length_ = SIZE_MAX / 2 + 1;
    }

    std::vector<std::vector<std::pair<Key, Value>>> new_list(length_);
    elems_ = 0;

    for (size_t i = 0; i < old_length; ++i) {
      for (const auto& pair_entry : list_[i]) {
        size_t new_bucket_idx = GetBucketIndexInternal(
            pair_entry.first, length_);  // 使用新的 length_
        new_list[new_bucket_idx].emplace_back(pair_entry.first,
                                              pair_entry.second);
        elems_++;
      }
    }
    list_ = std::move(new_list);
    // Now we need to move the temp_list_ to the new list
    std::lock_guard<std::mutex> lock(latch_);
    for (size_t i = 0; i < temp_list_.size(); ++i) {
      for (const auto& pair_entry : temp_list_[i]) {
        size_t new_bucket_idx =
            GetBucketIndexInternal(pair_entry.first, length_);
        list_[new_bucket_idx].emplace_back(pair_entry.first, pair_entry.second);
        elems_++;
      }
    }
    resizing_ = false;
  }

  auto SetSize(size_t size) -> void {
    if (size == 0) {
      size = 1;
    }
    length_ = 1;
    while (length_ < size) {
      length_ <<= 1;
    }
    list_.resize(length_);
  }

  auto Size() const -> size_t { return elems_; }

  auto Clear() -> void {
    for (auto& chain : list_) {
      chain.clear();
    }
    elems_ = 0;
    list_.assign(length_, std::vector<std::pair<Key, Value>>());
  }

  auto SetResizer(HashTableResizerType* resizer) -> void { resizer_ = resizer; }

 private:
  // The actual hash table
  std::vector<std::vector<std::pair<Key, Value>>> list_;
  // The number of buckets in the hash table
  size_t length_;
  // The number of elements in the hash table
  size_t elems_;
  // Temporary list for resizing
  std::vector<std::vector<std::pair<Key, Value>>> temp_list_;
  // Mutex for temp_list_
  std::mutex latch_;
  // Hash function
  HashFunc hash_function_;
  // Key equality function
  KeyEqualFunc key_equal_;
  // Pointer to the resizer
  HashTableResizerType* resizer_ = nullptr;
  // Flag to indicate if resizing is in progress
  std::atomic<bool> resizing_ = false;

  auto GetBucketIndex(const Key& key) const -> size_t {
    return hash_function_(key) & (length_ - 1);
  }

  auto GetBucketIndexInternal(const Key& key, size_t current_length) const
      -> size_t {
    return hash_function_(key) & (current_length - 1);
  }

  auto FindValuePtr(const Key& key) -> Value* {
    size_t bucket_idx = GetBucketIndex(key);
    std::vector<std::pair<Key, Value>>& chain = list_[bucket_idx];

    for (auto& pair_entry : chain) {
      if (key_equal_(pair_entry.first, key)) {
        return &(pair_entry.second);
      }
    }
    if (resizing_) {
      size_t temp_bucket_idx = GetBucketIndexInternal(key, temp_list_.size());
      std::vector<std::pair<Key, Value>>& temp_chain =
          temp_list_[temp_bucket_idx];

      for (auto& pair_entry : temp_chain) {
        if (key_equal_(pair_entry.first, key)) {
          return &(pair_entry.second);
        }
      }
    }
    return nullptr;
  }
};

}  // namespace myLru
