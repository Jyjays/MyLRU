#pragma once

#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>

#include "lru_cache.h"
#include "lru_cache_ht.h"
#include "config.h"

/**
 * args list: 
 */
struct TestParams {
    int num_threads;
    int total_operations;
    double size_ratio;
    size_t capacity_per_segment;
    TestParams(int threads, int operations, double ratio, size_t capacity)
        : num_threads(threads), total_operations(operations),
          size_ratio(ratio), capacity_per_segment(capacity) {}
};
int main(int argc, char** argv) {
    if (argc < 5) {
        std::cerr << "Usage: " << argv[0] << " <num_threads> <total_operations> "
                  << "<size_ratio> <capacity_per_segment>" << std::endl;
        return 1;
    }
    int num_threads = std::stoi(argv[1]);
    int total_operations = std::stoi(argv[2]);
    double size_ratio = std::stod(argv[3]);
    size_t capacity_per_segment = std::stoul(argv[4]);
    TestParams params(num_threads, total_operations, size_ratio, capacity_per_segment);
    std::cout << "Running tests with parameters: "
              << "num_threads=" << params.num_threads
              << ", total_operations=" << params.total_operations
              << ", size_ratio=" << params.size_ratio
              << ", capacity_per_segment=" << params.capacity_per_segment
              << std::endl;
    // Run tests
    
    
}