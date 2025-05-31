# 获取libcuckoo
git submodule add https://github.com/efficient/libcuckoo.git third_party/libcuckoo
git submodule update --init --recursive

# 获取gtest
git submodule add https://github.com/google/googletest.git third_party/googletest
git submodule update --init --recursive

mkdir build & cd build
cmake .. 
