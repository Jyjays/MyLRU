# CMake 最低版本要求 (libcuckoo 可能有自己的要求，3.15 是一个比较安全的选择)
cmake_minimum_required(VERSION 3.15)

# 项目名称和语言
project(MYLRU_Cache LANGUAGES CXX)

# 设置 C++ 标准 (libcuckoo 至少需要 C++11，建议使用更新的标准)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF) # 通常是好习惯，避免使用 GNU 扩展

# --- 添加 libcuckoo ---
add_subdirectory(third_party/libcuckoo)


# --- 定义可执行文件 ---
add_executable(mylru
    src/main.cpp  # 假设您的主程序入口文件
    src/lru/lru_cache.cpp # 假设您的 LRU Cache 实现
    src/hashtable/extendible_hash_table.cpp
)

# --- 为您的项目添加包含目录 ---
target_include_directories(mylru PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/include"
)

# --- 链接 libcuckoo ---
# 'cuckoohash' 是 libcuckoo 通过其 CMakeLists.txt 定义的 INTERFACE 库目标。
# 链接它会自动处理 libcuckoo 的包含目录。
target_link_libraries(mylru PRIVATE cuckoohash)

# --- 添加预处理器定义 ---
# 这将定义 USE_LIBCUCKOO 宏，以便 HashTableWrapper.h 选择 libcuckoo 实现。
target_compile_definitions(mylru PRIVATE USE_LIBCUCKOO)

# --- 查找并链接 Pthreads (线程库) ---
# libcuckoo 和您的并发 Cache 可能需要线程支持。
find_package(Threads REQUIRED)
target_link_libraries(mylru PRIVATE Threads::Threads)

# --- (可选) 安装可执行文件 ---
# install(TARGETS mylru DESTINATION bin)

# --- (可选) 启用测试 (如果您的项目中有 CTest 测试) ---
# enable_testing()
# add_test(NAME MyTest COMMAND mylru --some-test-args)

# 打印一些信息，方便调试
message(STATUS "Configuring MYLRU_Cache project")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Source directory: ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "Binary directory: ${CMAKE_CURRENT_BINARY_DIR}")
