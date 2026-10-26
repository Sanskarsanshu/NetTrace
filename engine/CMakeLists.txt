cmake_minimum_required(VERSION 3.20)
project(nettrace_engine VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warning flags
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
elseif (MSVC)
    add_compile_options(/W4 /permissive-)
endif()

# Core static library
add_library(nettrace_core STATIC
    src/model/error.cpp
    src/routing/graph.cpp
    src/routing/dijkstra.cpp
    src/routing/bfs.cpp
    src/routing/bellman_ford.cpp
    src/routing/widest_path.cpp
)

target_include_directories(nettrace_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Test executable
enable_testing()

add_executable(nettrace_tests
    tests/test_main.cpp
    tests/test_graph.cpp
    tests/test_dijkstra.cpp
    tests/test_bfs.cpp
    tests/test_bellman_ford.cpp
    tests/test_widest_path.cpp
)

target_include_directories(nettrace_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
)

target_link_libraries(nettrace_tests PRIVATE
    nettrace_core
)

add_test(NAME NetTrace_Algorithms_And_Graph_Tests COMMAND nettrace_tests)
