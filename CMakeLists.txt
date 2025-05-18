cmake_minimum_required(VERSION 3.16)

project(MYLRU_Cache LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

# 添加 libcuckoo（如果它的 CMakeLists.txt 需要）
add_subdirectory(third_party/libcuckoo)

# 定义可执行文件
# add_executable(mylru
#     src/main.cpp
#     src/lru/lru_cache.cpp 
# )

# # 添加包含目录
# target_include_directories(mylru PRIVATE
#     "${CMAKE_CURRENT_SOURCE_DIR}/src/include"
#     "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libcuckoo"  # 确保 libcuckoo 头文件路径
# )

# # 添加预处理器定义
# target_compile_definitions(mylru PRIVATE USE_LIBCUCKOO)

# 查找并链接 Pthreads
find_package(Threads REQUIRED)
# target_link_libraries(mylru PRIVATE Threads::Threads)

# 添加 Google Test
add_subdirectory(third_party/googletest)

# 定义单线程测试可执行文件
add_executable(mylru_tests
    test/single_thread_lru_test.cpp
    src/lru/lru_cache.cpp 
)

# 为单线程测试目标添加包含目录
target_include_directories(mylru_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/test"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libcuckoo"  # 确保 libcuckoo 头文件路径
)

# 为单线程测试目标添加预处理器定义
# target_compile_definitions(mylru_tests PRIVATE USE_LIBCUCKOO)

target_compile_definitions(mylru_tests PRIVATE USE_MY_HASH_TABLE)

# 链接 Google Test 和 Pthreads
target_link_libraries(mylru_tests PRIVATE
    gtest
    gtest_main
    Threads::Threads
)

# 定义多线程测试可执行文件
add_executable(mylru_tests_mt
    test/multi_thread_lru_test.cpp
    src/lru/lru_cache.cpp 
)

# 为多线程测试目标添加包含目录
target_include_directories(mylru_tests_mt PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/test"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libcuckoo"  # 确保 libcuckoo 头文件路径
)

# 为多线程测试目标添加预处理器定义
# target_compile_definitions(mylru_tests_mt PRIVATE USE_LIBCUCKOO)
target_compile_definitions(mylru_tests_mt PRIVATE USE_MY_HASH_TABLE)

# 链接 Google Test 和 Pthreads
target_link_libraries(mylru_tests_mt PRIVATE
    gtest
    gtest_main
    Threads::Threads
)

# 启用测试
enable_testing()
add_test(NAME MyLRUTests COMMAND mylru_tests)
add_test(NAME MyLRUMultiThreadTests COMMAND mylru_tests_mt)

# 打印调试信息
message(STATUS "Configuring MYLRU_Cache project")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Source directory: ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "Binary directory: ${CMAKE_CURRENT_BINARY_DIR}")