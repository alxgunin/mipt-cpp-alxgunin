cmake_minimum_required(VERSION 3.20)

project(CPP-tasks)

set(REQUIRED_CXX_COMPILER "Clang")
set(CXX_COMPILER_MIN_VERSION 14.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-stdlib=libc++)
add_link_options(-stdlib=libc++)

set(CMAKE_CXX_CLANG_TIDY clang-tidy-14)

add_executable(deque deque/deque_test_23.cpp)
add_executable(list list/stackallocator_test.cpp)
