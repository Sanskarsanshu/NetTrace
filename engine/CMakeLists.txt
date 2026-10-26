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

# Sanitizer option (used on Linux CI)
option(ENABLE_SANITIZERS "Enable Address and Undefined Behavior Sanitizers" OFF)
if (ENABLE_SANITIZERS)
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()

# Core static library
add_library(nettrace_core STATIC
    src/model/error.cpp
    src/model/ipv4.cpp
    src/model/mac.cpp
    src/model/topology.cpp
    src/model/validate.cpp
    src/model/broadcast_domain.cpp
    src/model/address_registry.cpp
    src/model/arp_cache.cpp
    src/model/mac_table.cpp
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
    tests/test_property.cpp
    tests/test_ipv4.cpp
    tests/test_mac.cpp
    tests/test_topology.cpp
    tests/test_switch_mac_table.cpp
    tests/test_address_registry.cpp
    tests/test_arp_cache.cpp
)

target_include_directories(nettrace_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
)

target_link_libraries(nettrace_tests PRIVATE
    nettrace_core
)

# Register each test individually with CTest
add_test(NAME T-1-01 COMMAND nettrace_tests "T-1-01")
add_test(NAME T-1-02 COMMAND nettrace_tests "T-1-02")
add_test(NAME T-1-03 COMMAND nettrace_tests "T-1-03")
add_test(NAME T-1-04 COMMAND nettrace_tests "T-1-04")
add_test(NAME T-1-05 COMMAND nettrace_tests "T-1-05")
add_test(NAME T-1-06 COMMAND nettrace_tests "T-1-06")
add_test(NAME T-1-07 COMMAND nettrace_tests "T-1-07")
add_test(NAME T-1-08 COMMAND nettrace_tests "T-1-08")
add_test(NAME T-1-09 COMMAND nettrace_tests "T-1-09")
add_test(NAME T-1-10 COMMAND nettrace_tests "T-1-10")
add_test(NAME T-1-11 COMMAND nettrace_tests "T-1-11")
add_test(NAME T-1-12 COMMAND nettrace_tests "T-1-12")
add_test(NAME T-1-13 COMMAND nettrace_tests "T-1-13")
add_test(NAME T-1-14 COMMAND nettrace_tests "T-1-14")
add_test(NAME T-1-15 COMMAND nettrace_tests "T-1-15")
add_test(NAME T-1-16 COMMAND nettrace_tests "T-1-16")
add_test(NAME T-1-17 COMMAND nettrace_tests "T-1-17")
add_test(NAME T-2-01 COMMAND nettrace_tests "T-2-01")
add_test(NAME T-2-02 COMMAND nettrace_tests "T-2-02")
add_test(NAME T-2-03 COMMAND nettrace_tests "T-2-03")
add_test(NAME T-2-16 COMMAND nettrace_tests "T-2-16")
add_test(NAME T-2-24 COMMAND nettrace_tests "T-2-24")
add_test(NAME T-2-12 COMMAND nettrace_tests "T-2-12")
add_test(NAME T-2-13 COMMAND nettrace_tests "T-2-13")
add_test(NAME T-2-17 COMMAND nettrace_tests "T-2-17")
add_test(NAME T-2-18 COMMAND nettrace_tests "T-2-18")
add_test(NAME T-2-19 COMMAND nettrace_tests "T-2-19")
add_test(NAME T-2-21 COMMAND nettrace_tests "T-2-21")
add_test(NAME T-2-23 COMMAND nettrace_tests "T-2-23")
add_test(NAME T-2-25 COMMAND nettrace_tests "T-2-25")


