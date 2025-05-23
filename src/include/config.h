#pragma once
#include <cstdint>
#include <functional>
#include <iostream>
#include <string>

#define USE_HASH_RESIZER



static const int kNumSegBits = 4;
static const int segNum = 1 << kNumSegBits;

static const int threadNum = 4;
static const int testsNum = 1000000;

static const double size_ratio = 0.1;

struct HashFuncImplement {
  size_t operator()(int64_t key) const noexcept {
    // uint64_t x = static_cast<uint64_t>(key);

    // // MurmurHash3 finalizer-like mixing (or FNV-1a inspired)
    // x ^= (x >> 33);
    // x *= 0xff51afd7ed558ccdULL;
    // x ^= (x >> 33);
    // x *= 0xc4ceb9fe1a85ec53ULL;
    // x ^= (x >> 33);

    // return static_cast<size_t>(x);
    return std::hash<int64_t>()(key);
  }
};

using KeyType = int64_t;
using ValueType = std::array<char, 16>;
using HashType = HashFuncImplement;
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

