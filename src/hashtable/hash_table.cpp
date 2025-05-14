// #include "hash_table.h"


// namespace myLru {

// MYHASH_ARGEMENT
// MYHASH_TABLE::MyHashTable() : length_(1 << kNumSegBits), elems_(0) {
//     list_.resize(length_);
// }


// MYHASH_ARGEMENT
// auto MYHASH_TABLE::Get(const Key& key, Value& value) -> bool {
//     auto v = FindValue(key, MyHash(key));
//     if (v == nullptr) {
//         return false;
//     }
//     value = v;
//     return true;
// }

// MYHASH_ARGEMENT
// auto MYHASH_TABLE::Insert(const Key& key, Value value) -> bool {
//     auto values = list_[MyHash(key) & (length_ - 1)];
//     for (auto iter : values) {
//         if (std::equal_to<Key>(*iter.first, key)) {
//             *iter.second = value;
//             return true;
//         }
//     }
//     values.push_back({key, value});
//     return true;
// }

// MYHASH_ARGEMENT
// auto MYHASH_TABLE::Remove(const Key& key) -> bool {
//     auto values = list_[MyHash(key) & (length_ - 1)];
//     for (auto iter = values.begin(); iter != values.end(); ++iter) {
//         if (std::equal_to<Key>(*iter.first, key)) {
//             values.erase(iter);
//             return true;
//         }
//     }
//     return false;
// }


// MYHASH_ARGEMENT
// auto MYHASH_TABLE::FindValue(const Key& key, size_t hash) -> Value* {
//     auto values = list_[hash & (length_ - 1)];
//     for (auto [k, v] : values) {
//       if (std::equal_to<Key>(key, k)) {
//         return &v;
//       }
//     }
//     return nullptr;
//   }
// }