#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <vector>

#include "hash_table.h"

namespace myLru {

#define DEFAULT_NUM_THREADS 1

template <typename Key, typename Value, typename HashFunc,
          typename KeyEqualFunc>
class HashTableResizer {
 public:
  using HashTableType = MyHashTable<Key, Value, HashFunc, KeyEqualFunc>;

  HashTableResizer() : stop_requested_(false) {
    //resize_thread_ = std::thread(&HashTableResizer::ResizeThread, this);
    printf("HashTableResizer: %d threads\n", DEFAULT_NUM_THREADS);
    for (int i = 0; i < DEFAULT_NUM_THREADS; ++i) {
      threads_.emplace_back(&HashTableResizer::ResizeThread, this);
    }
  }

  HashTableResizer(int size) : stop_requested_(false) {
    for (int i = 0; i < size; ++i) {
      threads_.emplace_back(&HashTableResizer::ResizeThread, this);
    }
  }

  ~HashTableResizer() {
    {
      std::unique_lock<std::mutex> lock(latch_);
      stop_requested_ = true;  
    }
    cv_.notify_all(); 
    for (auto &thread: threads_) {
      thread.join();
    }
  }

  // Prevent copying and assignment
  HashTableResizer(const HashTableResizer&) = delete;
  HashTableResizer& operator=(const HashTableResizer&) = delete;

  void EnqueueResize(HashTableType* table) {
    if (table == nullptr) return; 
    std::unique_lock<std::mutex> lock(latch_);
    resize_queue_.push(table);
    cv_.notify_one();
  }

 private:
  void ResizeThread() {
    while (true) {
      HashTableType* re_table = nullptr;
      {
        std::unique_lock<std::mutex> lock(latch_);
        cv_.wait(lock,
                 [this] { return !resize_queue_.empty() || stop_requested_; });

        if (stop_requested_ && resize_queue_.empty()) {
          break;
        }

        if (!resize_queue_.empty()) {
          re_table = resize_queue_.front();
          resize_queue_.pop();
        }
      }

      if (re_table != nullptr) {
        try {
          re_table
              ->Resize(); 
        } catch (const std::exception& e) {
            std::cerr << "Exception in resizing: " << e.what() << std::endl;
        }
      }
    }
  }

  std::mutex latch_;
  // Store raw pointers to MyHashTable instances that need resizing
  std::queue<HashTableType*> resize_queue_;
  // Condition variable to notify threads
  std::condition_variable cv_;
  // Flag to signal the thread to stop
  std::atomic<bool> stop_requested_;  
  // std::optional<std::thread> resize_thread_;
  std::vector<std::thread> threads_;
};

}  // namespace myLru
