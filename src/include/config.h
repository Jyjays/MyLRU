#include <cstdint>
#include <functional>


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