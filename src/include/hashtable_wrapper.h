#ifndef HASHTABLE_WRAPPER_H
#define HASHTABLE_WRAPPER_H

#ifdef USE_LIBCUCKOO
#include <libcuckoo/cuckoohash_map.hh>
#elif defined(USE_MY_HASH_TABLE)
#include "hash_table.h"
#elif defined(USE_SEG_HASH_TABLE)
#include "seg_hash_table.h"
#else
#define USE_LIBCUCKOO
#include <libcuckoo/cuckoohash_map.hh>
#endif

#include <functional>
#include <string>

namespace myLru {

template <typename Key, typename Value, typename Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class HashTableWrapper {
 public:
  using HashTableResizerType = HashTableResizer<Key, Value, Hash, KeyEqual>;
  HashTableWrapper() {
#ifdef USE_LIBCUCKOO
    // table_.set_num_buckets(16);
    // printf("Using libcuckoo hash table.\n");
#elif defined(USE_MY_HASH_TABLE)
    //printf("Using custom hash table implementation.\n");
    my_table_.SetSize(4096);
#elif defined(USE_SEG_HASH_TABLE)
    // printf("Using segmented hash table implementation.\n");
    my_table_.SetSize(4096);
#endif
  }

  /**
   * @brief Destructor.
   */
  ~HashTableWrapper() {
#ifdef USE_LIBCUCKOO
    table_.clear();
#elif defined(USE_MY_HASH_TABLE)
    my_table_.Clear();
    //printf("Using segmented hash table implementation.\n");
#elif defined(USE_SEG_HASH_TABLE)
    //printf("Using segmented hash table implementation.\n");
    my_table_.Clear();
#endif
  }

  HashTableWrapper(const HashTableWrapper&) = delete;
  HashTableWrapper& operator=(const HashTableWrapper&) = delete;

  // Allow move construction and move assignment
  HashTableWrapper(HashTableWrapper&&) = default;
  HashTableWrapper& operator=(HashTableWrapper&&) = default;

  auto Insert(const Key& key, const Value& value) -> bool {
#ifdef USE_LIBCUCKOO
    return table_.insert(key, value);
#elif defined(USE_MY_HASH_TABLE)
    return my_table_.Insert(key, value);
#elif defined(USE_SEG_HASH_TABLE)
    return my_table_.Insert(key, value);
#endif
  }

  auto Get(const Key& key, Value& value_out) -> bool {
#ifdef USE_LIBCUCKOO
    return table_.find(key, value_out);
#elif defined(USE_MY_HASH_TABLE)
    return my_table_.Get(key, value_out);
#elif defined(USE_SEG_HASH_TABLE)
    return my_table_.Get(key, value_out);
#endif
  }

  auto Remove(const Key& key) -> bool {
#ifdef USE_LIBCUCKOO
    return table_.erase(key);
#elif defined(USE_MY_HASH_TABLE)
    return my_table_.Remove(key);
#elif defined(USE_SEG_HASH_TABLE)
    return my_table_.Remove(key);
#endif
  }

  auto Size() const -> size_t {  // Marked const as it doesn't modify the table
#ifdef USE_LIBCUCKOO
    return table_.size();
#elif defined(USE_MY_HASH_TABLE)
    return my_table_.Size();
#elif defined(USE_SEG_HASH_TABLE)
    return my_table_.Size();
#endif
  }

  auto Clear() -> void {
#ifdef USE_LIBCUCKOO
    table_.clear();
#elif defined(USE_MY_HASH_TABLE)
    my_table_.Clear();
#elif defined(USE_SEG_HASH_TABLE)
    my_table_.Clear();
#endif
  }

  auto SetResizer(HashTableResizerType* resizer) -> void {
#ifdef USE_MY_HASH_TABLE
    my_table_.SetResizer(resizer);
#elif defined(USE_SEG_HASH_TABLE)
    my_table_.SetResizer(resizer);
#endif
  }

 private:
#ifdef USE_LIBCUCKOO
  libcuckoo::cuckoohash_map<Key, Value, Hash, KeyEqual> table_;
#elif defined(USE_MY_HASH_TABLE)
  MyHashTable<Key, Value, Hash, KeyEqual> my_table_;
#elif defined(USE_SEG_HASH_TABLE)
  SegHashTable<Key, Value, Hash, KeyEqual> my_table_;
#endif
};

}  // namespace myLru

#endif  // HASHTABLE_WRAPPER_H