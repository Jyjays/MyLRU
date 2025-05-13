#include <cstdint>
#include <functional>

static const int kNumSegBits = 4;
static const int segNum = 1 << kNumSegBits;

static const int threadNum = 8;
static const int testsNum = 1000000;


using KeyType = int64_t;
using ValueType = std::array<char, 16>;
using HashType = std::hash<KeyType>;
using KeyEqualType = std::equal_to<KeyType>;

#define LRU_ERR(msg) \
  do { \
    std::cerr << "Error: " << msg << std::endl; \
    exit(EXIT_FAILURE); \
  } while (0)
#define LRU_ASSERT(cond, msg) \
  do { \
    if (!(cond)) { \
      std::cerr << "Assertion failed: " << msg << std::endl; \
      exit(EXIT_FAILURE); \
    } \
  } while (0)