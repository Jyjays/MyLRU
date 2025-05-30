#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <thread>
#include <vector>

#include "lru_cache.h"
#include "lru_cache_ht.h"

namespace myLru {  // Using your namespace

ValueType generateValueForKey(KeyType key) {
  ValueType value{};
  std::memcpy(value.data(), &key, sizeof(KeyType));
  return value;
}

void printEvaluationResult(
    const std::string& test_name, int hit_count, int miss_count,
    std::chrono::high_resolution_clock::time_point start_time,  // 修改类型
    std::chrono::high_resolution_clock::time_point end_time,    // 修改类型
    long long total_operations  // 添加总操作数以便计算吞吐量
) {
  std::cout << "----------------------------------------" << std::endl;
  std::cout << "Test: " << test_name << std::endl;
  std::cout << "Hit Count: " << hit_count << std::endl;
  std::cout << "Miss Count: " << miss_count << std::endl;

  long long total_accesses = static_cast<long long>(hit_count) + miss_count;
  if (total_accesses > 0) {
    std::cout << std::fixed << std::setprecision(2);  // 设置输出精度
    std::cout << "Hit Ratio: "
              << static_cast<double>(hit_count) / total_accesses * 100 << "%"
              << std::endl;
    std::cout << "Miss Ratio: "
              << static_cast<double>(miss_count) / total_accesses * 100 << "%"
              << std::endl;
  } else {
    std::cout << "Hit Ratio: N/A (no accesses)" << std::endl;
    std::cout << "Miss Ratio: N/A (no accesses)" << std::endl;
  }

  auto elapsed_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);
  double elapsed_seconds = elapsed_duration.count() / 1000.0;

  double sim_disk_access_seconds =
      static_cast<double>(miss_count) * 0.0001;  // 模拟每个 miss 0.1ms
  double total_effective_seconds = elapsed_seconds + sim_disk_access_seconds;
  std::cout << "Effective Run Time (with simulated miss penalty): "
            << total_effective_seconds << " seconds" << std::endl;

  std::cout << "Actual Run Time: " << elapsed_seconds << " seconds"
            << std::endl;

  if (elapsed_seconds > 0) {
    double throughput = static_cast<double>(total_operations) / elapsed_seconds;
    std::cout << "Throughput: " << throughput << " ops/sec" << std::endl;
  } else {
    std::cout << "Throughput: N/A (run time too short or no operations)"
              << std::endl;
  }

  std::cout << "----------------------------------------" << std::endl;
}

// static const unsigned int COMMON_BASE_SEED = 8282347;

// --- Multi-threaded Insert and Find Test ---
TEST(SegLRUCacheMultiThreadTest, BenchMarkTest) {
  const int num_threads = threadNum;
  const int ops_per_thread = testsNum / num_threads;
  const size_t capacity = testsNum * size_ratio;
  LRUCache<KeyType, ValueType> cache(capacity);

  std::vector<std::thread> threads;
  std::atomic<int> successful_inserts(0);
  std::atomic<int> successful_finds(0);
  std::atomic<int> successful_removes(0);
  std::atomic<long long> attempted_finds(0);
  std::atomic<long long> attempted_inserts(0);
  std::atomic<long long> attempted_removes(0);

  // Key's range
  const KeyType max_key_value =
      static_cast<KeyType>(testsNum * size_ratio);

  std::chrono::high_resolution_clock::time_point chrono_start_time =
      std::chrono::high_resolution_clock::now();

  for (int i = 0; i < threadNum; i++) {
    threads.emplace_back([&, i]() {
      unsigned seed_for_thread = COMMON_BASE_SEED + i;
      std::mt19937_64 rng(seed_for_thread);  // 使用不同的种子
      std::uniform_int_distribution<KeyType> key_dist(0, max_key_value - 1);
      std::uniform_int_distribution<int> op_dist(0, 99);

      for (int j = 0; j < ops_per_thread; ++j) {
        KeyType key = key_dist(rng);
        int op_choice = op_dist(rng);

        if (op_choice < 45) {  // Insert operation
          attempted_inserts++;
          ValueType value = generateValueForKey(key);
          if (cache.Insert(key, value)) {
            successful_inserts++;
          }
        } else if (op_choice < 90) {  // Find operation
          attempted_finds++;
          ValueType retrieved_value;
          if (cache.Find(key, retrieved_value)) {
            successful_finds++;
          }
        } else {  // Remove operation
          attempted_removes++;
          if (cache.Remove(key)) {
            successful_removes++;
          }
        }
      }
    });
  }
  for (auto& t : threads) {
    t.join();
  }
  threads.clear();
  std::chrono::high_resolution_clock::time_point chrono_end_time =
      std::chrono::high_resolution_clock::now();
  long long total_executed_ops = attempted_inserts.load() +
                                 attempted_finds.load() +
                                 attempted_removes.load();
  long long current_attempted_finds = attempted_finds.load();
  long long current_successful_finds = successful_finds.load();
  long long current_miss_count =
      current_attempted_finds - current_successful_finds;
  printEvaluationResult("BenchMark Test (Single LRU)", successful_finds.load(),
                        current_miss_count, chrono_start_time, chrono_end_time,
                        total_executed_ops);
  EXPECT_GT(ops_per_thread * num_threads, 0);
  EXPECT_GT(successful_inserts.load(), 0);
  EXPECT_GT(successful_finds.load(), 0);
  EXPECT_GT(successful_removes.load(), 0);
}


TEST(SegLRUCacheMultiThreadTest, RandomizedMixedOperations) {
  const int num_threads = threadNum;
  const int ops_per_thread = testsNum / num_threads;
  const int total_capacity = testsNum * size_ratio;
  const size_t capacity_per_segment = total_capacity / segNum;
  const size_t actual_capacity_per_segment =
      (capacity_per_segment == 0) ? 1 : capacity_per_segment;

  SegLRUCache<KeyType, ValueType> cache(actual_capacity_per_segment);
  std::vector<std::thread> threads;
  std::atomic<int> successful_inserts(0);
  std::atomic<int> successful_finds(0);
  std::atomic<int> successful_removes(0);
  std::atomic<long long> attempted_finds(0);
  std::atomic<long long> attempted_inserts(0);
  std::atomic<long long> attempted_removes(0);

  const KeyType max_key_value =
      static_cast<KeyType>(testsNum * size_ratio);

  std::chrono::high_resolution_clock::time_point chrono_start_time =
      std::chrono::high_resolution_clock::now();

  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&, i]() {
      unsigned seed_for_thread = COMMON_BASE_SEED + i;
      std::mt19937_64 rng(seed_for_thread);
      std::uniform_int_distribution<KeyType> key_dist(0, max_key_value - 1);
      std::uniform_int_distribution<int> op_dist(0, 99);

      for (int j = 0; j < ops_per_thread; ++j) {
        KeyType key = key_dist(rng);
        int op_choice = op_dist(rng);

        if (op_choice < 45) {
          attempted_inserts++;
          ValueType value = generateValueForKey(key);
          if (cache.Insert(key, value)) {
            successful_inserts++;
          }
        } else if (op_choice < 90) {
          attempted_finds++;
          ValueType retrieved_value;
          if (cache.Find(key, retrieved_value)) {
            successful_finds++;
          }
        } else {
          attempted_removes++;
          if (cache.Remove(key)) {
            successful_removes++;
          }
        }
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  std::chrono::high_resolution_clock::time_point chrono_end_time =
      std::chrono::high_resolution_clock::now();

  long long total_executed_ops = attempted_inserts.load() +
                                 attempted_finds.load() +
                                 attempted_removes.load();
  long long current_attempted_finds = attempted_finds.load();
  long long current_successful_finds = successful_finds.load();
  long long current_miss_count =
      current_attempted_finds - current_successful_finds;

  printEvaluationResult("Randomized Mixed Operations Test (SegLRUCache)",
                        successful_finds.load(), current_miss_count,
                        chrono_start_time, chrono_end_time, total_executed_ops);

  EXPECT_GT(ops_per_thread * num_threads, 0);
}

TEST(SegLRUCacheMultiThreadTest, DISABLED_RandomizedMixedOperationsHT) {
  const int num_threads = threadNum;
  const int ops_per_thread = testsNum / num_threads;
  const int total_capacity = testsNum * size_ratio;
  const size_t capacity_per_segment = total_capacity / segNum;
  const size_t actual_capacity_per_segment =
      (capacity_per_segment == 0) ? 1 : capacity_per_segment;

  SegLRUCacheHT<KeyType, ValueType> cache(
      actual_capacity_per_segment);  // 使用 SegLRUCacheHT
  std::vector<std::thread> threads;
  std::atomic<int> successful_inserts(0);
  std::atomic<int> successful_finds(0);
  std::atomic<int> successful_removes(0);
  std::atomic<long long> attempted_finds(0);
  std::atomic<long long> attempted_inserts(0);
  std::atomic<long long> attempted_removes(0);

  const KeyType max_key_value =
      static_cast<KeyType>(testsNum * size_ratio);

  std::chrono::high_resolution_clock::time_point chrono_start_time =
      std::chrono::high_resolution_clock::now();

  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&, i]() {
      // 使用 COMMON_BASE_SEED 结合线程ID 'i' 来生成每个线程的种子
      unsigned seed_for_thread =
          COMMON_BASE_SEED + i;  // 与上面的测试使用相同的种子逻辑
      std::mt19937_64 rng(seed_for_thread);
      std::uniform_int_distribution<KeyType> key_dist(0, max_key_value - 1);
      std::uniform_int_distribution<int> op_dist(0, 99);

      for (int j = 0; j < ops_per_thread; ++j) {
        KeyType key = key_dist(rng);
        int op_choice = op_dist(rng);

        if (op_choice < 50) {
          attempted_inserts++;
          ValueType value = generateValueForKey(key);
          if (cache.Insert(key, value)) {
            successful_inserts++;
          }
        } else if (op_choice < 100) {
          attempted_finds++;
          ValueType retrieved_value;
          if (cache.Find(key, retrieved_value)) {
            successful_finds++;
          }
        } 
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  std::chrono::high_resolution_clock::time_point chrono_end_time =
      std::chrono::high_resolution_clock::now();

  long long total_executed_ops = attempted_inserts.load() +
                                 attempted_finds.load() +
                                 attempted_removes.load();
  long long current_attempted_finds = attempted_finds.load();
  long long current_successful_finds = successful_finds.load();
  long long current_miss_count =
      current_attempted_finds - current_successful_finds;

  printEvaluationResult("Randomized Mixed Operations Test (SegLRUCacheHT)",
                        successful_finds.load(), current_miss_count,
                        chrono_start_time, chrono_end_time, total_executed_ops);

  EXPECT_GT(ops_per_thread * num_threads, 0);
}
// --- Multi-threaded Mixed Operations Test (Insert, Find, Remove) ---
TEST(SegLRUCacheMultiThreadTest, DISABLED_ConcurrentMixedOperations) {
  const int num_threads = threadNum;  // More threads for higher contention
  const int ops_per_thread = testsNum / num_threads;
  const size_t capacity_per_segment = testsNum * size_ratio / segNum;
  std::cout << "capacity_per_segment: " << capacity_per_segment << std::endl;
  SegLRUCache<KeyType, ValueType> cache(capacity_per_segment);
  std::vector<std::thread> threads;
  std::atomic<int> successful_inserts(0);
  std::atomic<int> hit_count(0);
  std::atomic<int> miss_count(0);

  // 使用 chrono 进行计时
  std::chrono::high_resolution_clock::time_point chrono_start_time =
      std::chrono::high_resolution_clock::now();

  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&, i]() {  // Capture by reference, i by value
      for (int j = 0; j < ops_per_thread; ++j) {
        KeyType key = static_cast<KeyType>(
            (i * ops_per_thread + j) %
            (num_threads * ops_per_thread / 2));  // Keys will overlap

        ValueType value = generateValueForKey(key);
        if (cache.Insert(key, value)) {
          successful_inserts++;
        }
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  threads.clear();

  // Phase 2: Concurrent Finds and Removes
  long long total_find_ops_attempted = 0;
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&, i]() {  // Capture by reference, i by value
      for (int j = 0; j < ops_per_thread; ++j) {
        KeyType key = static_cast<KeyType>(
            (i * ops_per_thread + j) %
            (num_threads * ops_per_thread / 2));  // Keys will overlap

        ValueType retrieved_value;
        total_find_ops_attempted++;
        if (cache.Find(key, retrieved_value)) {
          hit_count++;
          // if (cache.Remove(key)) {
          //   successful_removes++;
          // }
        } else {
          miss_count++;
        }
      }
    });
  }
  for (auto& t : threads) {
    t.join();
  }
  threads.clear();

  std::chrono::high_resolution_clock::time_point chrono_end_time =
      std::chrono::high_resolution_clock::now();

  long long total_measured_operations =
      successful_inserts.load() + total_find_ops_attempted;

  printEvaluationResult("Concurrent Mixed Operations Test", hit_count.load(),
                        miss_count.load(), chrono_start_time, chrono_end_time,
                        total_measured_operations);

  EXPECT_GT(successful_inserts.load(), 0);
}
// --- Multi-threaded Mixed Operations Test (Insert, Find, Remove) ---
TEST(SegLRUCacheMultiThreadTest, DISABLED_ConcurrentMixedOperationsHT) {
  const int num_threads = threadNum;  // More threads for higher contention
  const int ops_per_thread = testsNum / num_threads;
  const size_t capacity_per_segment = testsNum * size_ratio / segNum;

  SegLRUCacheHT<KeyType, ValueType> cache(capacity_per_segment);
  std::vector<std::thread> threads;
  std::atomic<int> successful_inserts(0);
  std::atomic<int> hit_count(0);
  std::atomic<int> miss_count(0);

  std::chrono::high_resolution_clock::time_point chrono_start_time =
      std::chrono::high_resolution_clock::now();

  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&, i]() {  // Capture by reference, i by value
      for (int j = 0; j < ops_per_thread; ++j) {
        KeyType key = static_cast<KeyType>(
            (i * ops_per_thread + j) %
            (num_threads * ops_per_thread / 2));  // Keys will overlap

        ValueType value = generateValueForKey(key);
        if (cache.Insert(key, value)) {
          successful_inserts++;
        }
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  threads.clear();

  // Phase 2: Concurrent Finds and Removes
  long long total_find_ops_attempted = 0;
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&, i]() {  // Capture by reference, i by value
      for (int j = 0; j < ops_per_thread; ++j) {
        KeyType key = static_cast<KeyType>(
            (i * ops_per_thread + j) %
            (num_threads * ops_per_thread / 2));  // Keys will overlap

        ValueType retrieved_value;
        total_find_ops_attempted++;
        if (cache.Find(key, retrieved_value)) {
          hit_count++;
          // if (cache.Remove(key)) {
          //   successful_removes++;
          // }
        } else {
          miss_count++;
        }
      }
    });
  }
  for (auto& t : threads) {
    t.join();
  }
  threads.clear();

  std::chrono::high_resolution_clock::time_point chrono_end_time =
      std::chrono::high_resolution_clock::now();

  long long total_measured_operations =
      successful_inserts.load() + total_find_ops_attempted;

  printEvaluationResult("HT Concurrent Mixed Operations Test", hit_count.load(),
                        miss_count.load(), chrono_start_time, chrono_end_time,
                        total_measured_operations);

  EXPECT_GT(successful_inserts.load(), 0);
}

}  // namespace myLru 