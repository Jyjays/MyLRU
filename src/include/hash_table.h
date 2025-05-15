#pragma once

#include <vector>
#include <functional> 
#include <utility>    
#include <stdexcept>  


namespace myLru {

template <typename Key,
          typename Value,
          typename HashFunc = std::hash<Key>,
          typename KeyEqualFunc = std::equal_to<Key>>
class MyHashTable {
public:

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

    bool Get(const Key& key, Value& value_out) {
        Value* found_value_ptr = FindValuePtr(key); 
        if (found_value_ptr != nullptr) {
            value_out = *found_value_ptr; 
            return true;
        }
        return false;
    }


    bool Insert(const Key& key, Value value_to_insert) {
        size_t bucket_idx = GetBucketIndex(key);
        std::vector<std::pair<Key, Value>>& chain = list_[bucket_idx];

        for (auto& pair_entry : chain) { 
            if (key_equal_(pair_entry.first, key)) {
                pair_entry.second = value_to_insert;
                return true;
            }
        }

        chain.emplace_back(key, value_to_insert);
        elems_++;
        if (elems_ > length_) {
            Resize();
        }
        return true;
    }

    bool Remove(const Key& key) {
        size_t bucket_idx = GetBucketIndex(key);
        std::vector<std::pair<Key, Value>>& chain = list_[bucket_idx];

        for (auto iter = chain.begin(); iter != chain.end(); ++iter) {
            if (key_equal_(iter->first, key)) {
                chain.erase(iter); // 从链中移除
                elems_--;
                return true;
            }
        }
        return false;
    }

    void Resize() {
        size_t old_length = length_;
        length_ <<= 1;
        if (length_ == 0) { 
             length_ = old_length > 0 ? old_length * 2 : 16; 
             if (length_ == 0) length_ = SIZE_MAX / 2 +1;
        }

        std::vector<std::vector<std::pair<Key, Value>>> new_list(length_);
        elems_ = 0; 

        for (size_t i = 0; i < old_length; ++i) {
            for (const auto& pair_entry : list_[i]) {
                size_t new_bucket_idx = GetBucketIndexInternal(pair_entry.first, length_); // 使用新的 length_
                new_list[new_bucket_idx].emplace_back(pair_entry.first, pair_entry.second);
                elems_++;
            }
        }
        list_ = std::move(new_list);
    }

    size_t Size() const {
        return elems_;
    }

    void Clear() {
        for(auto& chain : list_) {
            chain.clear();
        }
        elems_ = 0;
        list_.assign(length_, std::vector<std::pair<Key, Value>>()); 
    }


private:
    std::vector<std::vector<std::pair<Key, Value>>> list_;
    size_t length_;
    size_t elems_;  

    HashFunc hash_function_;      
    KeyEqualFunc key_equal_;     

    size_t GetBucketIndex(const Key& key) const {
        return hash_function_(key) & (length_ - 1); 
    }

    size_t GetBucketIndexInternal(const Key& key, size_t current_length) const {
        return hash_function_(key) & (current_length - 1);
    }


    Value* FindValuePtr(const Key& key) {
        size_t bucket_idx = GetBucketIndex(key);
        std::vector<std::pair<Key, Value>>& chain = list_[bucket_idx];

        for (auto& pair_entry : chain) {
            if (key_equal_(pair_entry.first, key)) {
                return &(pair_entry.second);
            }
        }
        return nullptr;
    }
};

} // namespace myLru
