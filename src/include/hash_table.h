#pragma once

#include <condition_variable>
#include <functional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "config.h"

namespace myLru {

template <typename K, typename V, typename HF = HashFuncImplement,
          typename KEF = std::equal_to<K>>
class HashTableResizer;

template <typename Key, typename Value, typename HashFunc = HashFuncImplement,
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
    std::unique_lock<std::mutex> lock(read_latch_);
    size_t bucket_idx = GetBucketIndex(key);
    std::vector<std::pair<Key, Value>>& chain = list_[bucket_idx];

    for (auto& pair_entry : chain) {
      if (key_equal_(pair_entry.first, key)) {
        // pair_entry.second = value_to_insert;
        // return true;
        // Not allow to update the value
        return false;
      }
    }
    if (resizing_) {
      // If resizing, check in the temp list
      size_t temp_bucket_idx = GetBucketIndexInternal(key, temp_list_size);
      std::vector<std::pair<Key, Value>>& temp_chain =
          temp_list_[temp_bucket_idx];
      for (auto& pair_entry : temp_chain) {
        if (key_equal_(pair_entry.first, key)) {
          return false;
        }
      }
      // If not found, insert into the temp list.
      temp_chain.emplace_back(key, value_to_insert);
      return true;
    } else {
      chain.emplace_back(key, value_to_insert);
      elems_++;
    }
    // lock.unlock();
    // When the number of elements is more than 2 times the length,
    // we need to resize the hash table.
    if (elems_ > 2 * length_) {
      if (resizer_ != nullptr) {
        resizing_ = true;
        initialize_temp_list();
        resizer_->EnqueueResize(this);
      } else {
        lock.unlock();
        Resize();
      }
    }
    return true;
  }

  auto Remove(const Key& key) -> bool {
    std::unique_lock<std::mutex> lock(read_latch_);
    size_t bucket_idx = GetBucketIndex(key);

    if (resizing_) {
      // std::lock_guard<std::mutex> lock(latch_);
      size_t temp_bucket_idx = GetBucketIndexInternal(key, temp_list_size);
      std::vector<std::pair<Key, Value>>& temp_chain =
          temp_list_[temp_bucket_idx];
      for (auto& entry : temp_chain) {
        if (key_equal_(entry.first, key)) {
          temp_chain.erase(
              std::remove(temp_chain.begin(), temp_chain.end(), entry),
              temp_chain.end());
          elems_--;
          return true;
        }
      }
    }
    std::vector<std::pair<Key, Value>>& chain = list_[bucket_idx];
    for (auto& entry : chain) {
      if (key_equal_(entry.first, key)) {
        chain.erase(std::remove(chain.begin(), chain.end(), entry),
                    chain.end());
        elems_--;
        return true;
      }
    }
    return false;
  }

  auto Resize() -> void {
    size_t new_length_ = length_ << 1;
    std::vector<std::vector<std::pair<Key, Value>>> new_list(new_length_);
    // Copy the old elements to the new list
    for (size_t i = 0; i < length_; i++) {
      std::vector<std::pair<Key, Value>>& chain = list_[i];

      for (auto& entry : chain) {
        size_t new_bucket_idx =
            GetBucketIndexInternal(entry.first, new_length_);
        new_list[new_bucket_idx].emplace_back(entry.first, entry.second);
      }
    }
    std::lock_guard<std::mutex> lock(read_latch_);
    list_ = std::move(new_list);
    length_ = new_length_;
    resizing_ = false;
    // Insert the elements from the temp list to new list
    // std::lock_guard<std::mutex> lock(latch_);
    if (temp_list_.empty() || resizer_ == nullptr) {
      return;
    }
    for (size_t i = 0; i < temp_list_size; i++) {
      auto& temp_chain = temp_list_[i];
      for (auto& entry : temp_chain) {
        size_t new_bucket_idx =
            GetBucketIndexInternal(entry.first, new_length_);
        list_[new_bucket_idx].emplace_back(entry.first, entry.second);
      }
    }
    // Clear the temp list after the resizing set to false
    temp_list_.clear();
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
  // Mutex for list_
  std::mutex read_latch_;
  // The number of buckets in the hash table
  size_t length_;
  // The number of elements in the hash table
  size_t elems_;
  // Temporary list for resizing
  std::vector<std::vector<std::pair<Key, Value>>> temp_list_;
  // Mutex for temp_list_
  // std::mutex latch_;
  // Hash function
  HashFunc hash_function_;
  // Key equality function
  KeyEqualFunc key_equal_;
  // Pointer to the resizer
  HashTableResizerType* resizer_ = nullptr;
  // Flag to indicate if resizing is in progress
  std::atomic<bool> resizing_ = false;
  // The size of the temporary list
  static const size_t temp_list_size = 8;

  auto GetBucketIndex(const Key& key) const -> size_t {
    return hash_function_(key) & (length_ - 1);
  }

  auto GetBucketIndexInternal(const Key& key, size_t current_length) const
      -> size_t {
    return hash_function_(key) & (current_length - 1);
  }

  auto FindValuePtr(const Key& key) -> Value* {
    std::unique_lock<std::mutex> lock(read_latch_);
    size_t bucket_idx = GetBucketIndex(key);
    std::vector<std::pair<Key, Value>>& chain = list_[bucket_idx];

    for (auto& pair_entry : chain) {
      if (key_equal_(pair_entry.first, key)) {
        return &(pair_entry.second);
      }
    }
    if (resizing_) {
      size_t temp_bucket_idx = GetBucketIndexInternal(key, temp_list_size);
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

  inline auto initialize_temp_list() -> void {
    temp_list_.resize(temp_list_size);
  }
};

}  // namespace myLru
