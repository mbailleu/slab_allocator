cmake_minimum_required(VERSION 3.16)

project(SlabAlloc VERSION 0.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(test
        test.cpp
)

target_compile_options(test PRIVATE -DALLOCATE_STATS=1 -DSLAB_ALLOC_CHECK_DOUBLE_FREE=1)
