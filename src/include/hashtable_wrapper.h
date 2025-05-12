#ifndef HASHTABLE_WRAPPER_H
#define HASHTABLE_WRAPPER_H

// =====================================================================================
// Preprocessor Directives for Hash Table Implementation Choice
// =====================================================================================
// Define ONE of these macros in your build system (e.g., CMakeLists.txt, Makefile, or IDE project settings)
// or define it here directly before including this header for testing.
//
// Example:
// #define USE_LIBCUCKOO
// #define USE_MY_EXTENDIBLE_HASH_TABLE

// For this initial version, we'll assume USE_LIBCUCKOO is defined for libcuckoo.
// If you define USE_MY_EXTENDIBLE_HASH_TABLE, you'll need to provide its implementation.

// =====================================================================================
// Conditional Includes
// =====================================================================================
#ifdef USE_LIBCUCKOO
    // Include the libcuckoo hash map header.
    // Make sure libcuckoo is in your include path and linked correctly.
    // You can get libcuckoo from: https://github.com/efficient/libcuckoo
    #include <libcuckoo/cuckoohash_map.hh>
#elif defined(USE_MY_EXTENDIBLE_HASH_TABLE)
    // Placeholder for your custom extendible hash table header
    // #include "my_extendible_hash_table.h" // You will create this file
    // For now, we'll add a compile-time error if this is selected but not implemented.
    #error "USE_MY_EXTENDIBLE_HASH_TABLE is defined, but the implementation is not yet provided in this wrapper."
#else
    // Default to libcuckoo if nothing is explicitly defined, or throw an error.
    // For clarity, it's better to require explicit definition.
    #warning "No specific hash table implementation selected. Defaulting to USE_LIBCUCKOO. Define USE_LIBCUCKOO or USE_MY_EXTENDIBLE_HASH_TABLE explicitly."
    #define USE_LIBCUCKOO
    #include <libcuckoo/cuckoohash_map.hh>
    // Alternatively, you could throw an error:
    // #error "No hash table implementation selected. Define USE_LIBCUCKOO or USE_MY_EXTENDIBLE_HASH_TABLE."
#endif

#include <functional> // For std::hash, std::equal_to
#include <string>     // For potential key types, though templates make it generic

// Forward declaration if your Value type is complex and needs it (e.g., pointer to LRU node)
// template <typename Key, typename Value> struct LruNode;


/**
 * @brief A wrapper class for hash table implementations.
 *
 * This class provides a common interface (Insert, Get, Remove) for different
 * underlying hash table implementations. The choice of implementation can be
 * controlled via preprocessor directives (USE_LIBCUCKOO or USE_MY_EXTENDIBLE_HASH_TABLE).
 *
 * It is designed to be thread-safe if the underlying hash table implementation
 * (like libcuckoo) is thread-safe.
 *
 * @tparam Key The type of the keys.
 * @tparam Value The type of the values.
 * @tparam Hash The hash function for the Key (defaults to std::hash<Key>).
 * @tparam KeyEqual The equality comparison function for the Key (defaults to std::equal_to<Key>).
 */
template <typename Key,
          typename Value,
          typename Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class HashTableWrapper {
public:
    /**
     * @brief Default constructor.
     * Initializes the underlying hash table.
     * For libcuckoo, this might involve specifying an initial capacity.
     */
    HashTableWrapper() {
        // libcuckoo::cuckoohash_map can take an initial capacity as an argument.
        // Example: table_(1024); // Initialize with a capacity of 1024
        // If not specified, it uses a default initial capacity.
        // For your extendible hash table, you'll call its constructor here.
    }

    /**
     * @brief Destructor.
     */
    ~HashTableWrapper() {
        // Underlying hash table will be destructed automatically.
    }

    // Delete copy constructor and copy assignment operator to prevent accidental copies
    // if the underlying table (like libcuckoo's map) is not trivially copyable
    // or if copying is an expensive operation you want to control.
    // libcuckoo::cuckoohash_map is not copyable.
    HashTableWrapper(const HashTableWrapper&) = delete;
    HashTableWrapper& operator=(const HashTableWrapper&) = delete;

    // Allow move construction and move assignment
    HashTableWrapper(HashTableWrapper&&) = default;
    HashTableWrapper& operator=(HashTableWrapper&&) = default;


    /**
     * @brief Inserts a key-value pair into the hash table.
     *
     * @param key The key to insert.
     * @param value The value associated with the key.
     * @return True if the insertion was successful (e.g., key was new).
     * False if the key already existed or an error occurred.
     * (libcuckoo's insert returns true on success, false if key existed)
     */
    bool Insert(const Key& key, const Value& value) {
#ifdef USE_LIBCUCKOO
        return table_.insert(key, value);
#elif defined(USE_MY_EXTENDIBLE_HASH_TABLE)
        // return my_table_.Insert(key, value); // Adapt to your EHT API
        static_assert(false, "Insert not implemented for USE_MY_EXTENDIBLE_HASH_TABLE yet");
        return false; // Placeholder
#endif
    }

    /**
     * @brief Retrieves the value associated with a given key.
     *
     * @param key The key to search for.
     * @param value_out [out] If the key is found, this parameter is populated with the value.
     * @return True if the key was found and value_out is populated, false otherwise.
     */
    bool Get(const Key& key, Value& value_out) {
#ifdef USE_LIBCUCKOO
        return table_.find(key, value_out);
#elif defined(USE_MY_EXTENDIBLE_HASH_TABLE)
        // return my_table_.Get(key, value_out); // Adapt to your EHT API
        static_assert(false, "Get not implemented for USE_MY_EXTENDIBLE_HASH_TABLE yet");
        return false; // Placeholder
#endif
    }

    /**
     * @brief Removes a key-value pair from the hash table.
     *
     * @param key The key to remove.
     * @return True if the key was found and removed, false otherwise.
     */
    bool Remove(const Key& key) {
#ifdef USE_LIBCUCKOO
        return table_.erase(key);
#elif defined(USE_MY_EXTENDIBLE_HASH_TABLE)
        // return my_table_.Remove(key); // Adapt to your EHT API
        static_assert(false, "Remove not implemented for USE_MY_EXTENDIBLE_HASH_TABLE yet");
        return false; // Placeholder
#endif
    }

    /**
     * @brief Returns the number of key-value pairs currently in the hash table.
     *
     * @return The size of the hash table.
     */
    size_t Size() const { // Marked const as it doesn't modify the table
#ifdef USE_LIBCUCKOO
        return table_.size();
#elif defined(USE_MY_EXTENDIBLE_HASH_TABLE)
        // return my_table_.Size(); // Adapt to your EHT API
        static_assert(false, "Size not implemented for USE_MY_EXTENDIBLE_HASH_TABLE yet");
        return 0; // Placeholder
#endif
    }

    /**
     * @brief Removes all elements from the hash table.
     */
    void Clear() {
#ifdef USE_LIBCUCKOO
        table_.clear();
#elif defined(USE_MY_EXTENDIBLE_HASH_TABLE)
        // my_table_.Clear(); // Adapt to your EHT API
        static_assert(false, "Clear not implemented for USE_MY_EXTENDIBLE_HASH_TABLE yet");
#endif
    }

    /**
     * @brief (Optional) Provides locked access to the table for complex operations.
     * This is an advanced feature. libcuckoo provides `locked_table` for this.
     *
     * Example:
     * auto locked_table = table_.lock_table(); // For libcuckoo
     * // Perform multiple operations on locked_table
     * // The lock is released when locked_table goes out of scope.
     *
     * You might expose this or provide higher-level atomic operations if needed.
     * For now, this is commented out as it's specific to libcuckoo's advanced API.
     */
    /*
    auto GetLockedTable() {
    #ifdef USE_LIBCUCKOO
        return table_.lock_table();
    #else
        // Your custom hash table might need a different mechanism or might not support it.
        // Consider returning a proxy object that holds a lock.
        return nullptr; // Placeholder
    #endif
    }
    */

private:
    // The actual hash table instance
#ifdef USE_LIBCUCKOO
    libcuckoo::cuckoohash_map<Key, Value, Hash, KeyEqual> table_;
#elif defined(USE_MY_EXTENDIBLE_HASH_TABLE)
    // MyExtendibleHashTable<Key, Value, Hash, KeyEqual> my_table_; // Your implementation
#endif

    // Note on Thread Safety:
    // libcuckoo::cuckoohash_map is designed to be thread-safe for concurrent
    // reads, writes, and resizes. Therefore, this wrapper, when using libcuckoo,
    // will also be thread-safe for the implemented Insert, Get, Remove, Size, Clear operations
    // without needing additional mutexes around these individual calls.
    // If you implement USE_MY_EXTENDIBLE_HASH_TABLE, you will be responsible for
    // ensuring its thread safety or adding appropriate synchronization mechanisms
    // either within your extendible hash table class or in this wrapper.
};

#endif // HASHTABLE_WRAPPER_H

