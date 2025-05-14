#ifndef MY_HASH_TABLE_H
#define MY_HASH_TABLE_H

#include <vector>
#include <functional> // For std::hash, std::equal_to
#include <utility>    // For std::pair
#include <stdexcept>  // For std::out_of_range (optional error handling)

// 假设 KeyType, ValueType 已经通过某种方式定义 (例如在 config.h 中)
// 在 LRUCache 的上下文中, MyHashTable 的模板参数 Value 将会是 LRUNode*

// 如果您的 MYHASH_ARGEMENT 和 MYHASH_TABLE 宏有特定定义，请确保它们与模板类兼容
// 通常对于头文件中的模板定义，这些宏可能不需要，或者需要调整。
// 为简单起见，我将直接在类内部定义模板成员。
// #define MYHASH_ARGEMENT template <typename Key, typename Value, typename HashFunc, typename KeyEqualFunc>
// #define MYHASH_TABLE MyHashTable<Key, Value, HashFunc, KeyEqualFunc>


namespace myLru {

template <typename Key,
          typename Value, // 当用于LRUCache时, Value 是 LRUNode*
          typename HashFunc = std::hash<Key>,
          typename KeyEqualFunc = std::equal_to<Key>>
class MyHashTable {
public:
    // 构造函数，提供一个默认的初始桶数量
    explicit MyHashTable(size_t initial_buckets = 16) : elems_(0) {
        if (initial_buckets == 0) {
            initial_buckets = 1; // 防止桶数量为0
        }
        // 通常希望桶数量是2的幂，以便使用位运算代替取模
        length_ = 1;
        while (length_ < initial_buckets) {
            length_ <<= 1;
        }
        list_.resize(length_);
    }

    // Get: 查找与 key 关联的 value (在LRUCache中是 LRUNode*)。
    // value_out 是对 Value (LRUNode*) 的引用，所以它会被更新。
    bool Get(const Key& key, Value& value_out) {
        Value* found_value_ptr = FindValuePtr(key); // FindValuePtr 返回 Value* (即 LRUNode**)
        if (found_value_ptr != nullptr) {
            value_out = *found_value_ptr; // 解引用以获取 Value (LRUNode*)
            return true;
        }
        return false;
    }

    // Insert: 插入一个键值对 (Key, LRUNode*)。
    // 如果键已存在，则更新其值。
    // 返回 true 表示成功插入或更新。
    bool Insert(const Key& key, Value value_to_insert) { // value_to_insert 是 LRUNode*
        // TODO: 在插入前检查加载因子并调用 Resize()
        // if (static_cast<double>(elems_ + 1) / length_ > 0.75) {
        //     Resize();
        // }

        size_t bucket_idx = GetBucketIndex(key);
        std::vector<std::pair<Key, Value>>& chain = list_[bucket_idx];

        for (auto& pair_entry : chain) { // 使用引用以允许修改
            if (key_equal_(pair_entry.first, key)) {
                pair_entry.second = value_to_insert; // 更新已存在键的值 (LRUNode*)
                return true;
            }
        }

        // 键不存在，插入新的键值对
        chain.emplace_back(key, value_to_insert);
        elems_++;
        return true;
    }

    // Remove:移除与 key 关联的键值对。
    // 返回 true 表示成功移除。
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
        return false; // 未找到要移除的键
    }

    // Resize: 调整哈希表的大小 (重新哈希所有元素)
    void Resize() {
        size_t old_length = length_;
        length_ <<= 1; // 将桶数量翻倍 (简单策略)
        if (length_ == 0) { // 防止 length_ 溢出后变为0 (虽然不太可能，但作为保护)
             length_ = old_length > 0 ? old_length * 2 : 16; // 恢复或设为默认
             if (length_ == 0) length_ = SIZE_MAX / 2 +1; // 极端情况
        }


        std::vector<std::vector<std::pair<Key, Value>>> new_list(length_);
        elems_ = 0; // 重置元素计数，将在重新插入时更新

        for (size_t i = 0; i < old_length; ++i) {
            for (const auto& pair_entry : list_[i]) {
                // 重新哈希并插入到 new_list
                size_t new_bucket_idx = GetBucketIndexInternal(pair_entry.first, length_); // 使用新的 length_
                new_list[new_bucket_idx].emplace_back(pair_entry.first, pair_entry.second);
                elems_++;
            }
        }
        list_ = std::move(new_list); // 用新的列表替换旧的
    }

    size_t Size() const {
        return elems_;
    }

    void Clear() {
        for(auto& chain : list_) {
            chain.clear();
        }
        elems_ = 0;
        // 可以选择是否将 list_ resize回初始大小，或者保持当前桶数
        // list_.assign(length_, std::vector<std::pair<Key, Value>>()); // 清空所有链
    }


private:
    std::vector<std::vector<std::pair<Key, Value>>> list_; // 桶数组，每个桶是一个键值对链表
    size_t length_; // 哈希桶的数量 (list_.size())
    size_t elems_;  // 哈希表中存储的元素总数

    HashFunc hash_function_;       // 哈希函数对象实例
    KeyEqualFunc key_equal_;       // 键比较函数对象实例

    // 内部辅助函数：计算哈希值并返回桶索引
    size_t GetBucketIndex(const Key& key) const {
        return hash_function_(key) & (length_ - 1); // 假设 length_ 总是2的幂
    }

    // 内部辅助函数：在 Resize 时使用新的 length 计算桶索引
    size_t GetBucketIndexInternal(const Key& key, size_t current_length) const {
        return hash_function_(key) & (current_length - 1);
    }


    // FindValuePtr: 查找 key，如果找到则返回指向存储的 Value (LRUNode*) 的指针，否则返回 nullptr。
    // 返回的是 Value* (例如 LRUNode**)，因为 std::pair<Key, Value> 中的 Value 是 LRUNode*
    Value* FindValuePtr(const Key& key) {
        size_t bucket_idx = GetBucketIndex(key);
        std::vector<std::pair<Key, Value>>& chain = list_[bucket_idx]; // 需要可修改的引用，以便返回非 const 指针

        for (auto& pair_entry : chain) { // 迭代器需要是非 const 的
            if (key_equal_(pair_entry.first, key)) {
                return &(pair_entry.second); // 返回存储在 pair 中的 Value (LRUNode*) 的地址
            }
        }
        return nullptr;
    }
};

} // namespace myLru

#endif // MY_HASH_TABLE_H