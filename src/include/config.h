#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <iostream>
#include <string>

static const int kNumSegBits = 4;
static const int segNum = 1 << kNumSegBits;

static const int threadNum = 8;
static const int testsNum = 1000000;

static const double size_ratio = 0.3;

static const unsigned int COMMON_BASE_SEED = 8282347;

struct ShardHashFunc {
  size_t operator()(int64_t key) const noexcept {
    return std::hash<int64_t>()(key);
  }
};

struct HashFuncImpl {
  size_t operator()(int64_t key) const noexcept {
    uint64_t k = static_cast<uint64_t>(key);
    // k = (~k) + (k << 21);  // k = (k << 21) - k - 1;
    // k = k ^ (k >> 24);
    // k = (k + (k << 3)) + (k << 8);  // k * 265
    // k = k ^ (k >> 14);
    // k = (k + (k << 2)) + (k << 4);  // k * 21
    // k = k ^ (k >> 28);
    // k = k + (k << 31);
    // return static_cast<size_t>(k);
    // return std::hash<int64_t>()(key);
    uint64_t x = static_cast<uint64_t>(key);
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    x = x ^ (x >> 31);
    return static_cast<size_t>(x);
  }
};

using KeyType = int64_t;
using ValueType = std::array<char, 16>;
using HashType = HashFuncImpl;
using KeyEqualType = std::equal_to<KeyType>;

#define LRU_ERR(msg)                            \
  do {                                          \
    std::cerr << "Error: " << msg << std::endl; \
    exit(EXIT_FAILURE);                         \
  } while (0)
#define LRU_ASSERT(cond, msg)                                \
                                                             \
  do {                                                       \
    if (!(cond)) {                                           \
      std::cerr << "Assertion failed: " << msg << std::endl; \
      exit(EXIT_FAILURE);                                    \
    }                                                        \
  } while (0)
