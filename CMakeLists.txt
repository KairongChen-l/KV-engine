cmake_minimum_required(VERSION 3.29)
project(KVcache)

set(CMAKE_CXX_STANDARD 20)

add_executable(KVcache main.cpp
        skiplist.h)
