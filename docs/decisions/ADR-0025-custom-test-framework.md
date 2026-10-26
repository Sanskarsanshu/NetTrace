# ADR-0025: Embedded Custom C++ Unit Test Framework

## Status
Accepted

## Context
Milestone 1 implements graph data structures, validation, and core routing algorithms (Dijkstra, BFS, Bellman-Ford, Widest-Path). We evaluated integrating an external C++ unit test framework (such as GoogleTest or Catch2) versus retaining the project's zero-dependency lightweight embedded test framework (`engine/tests/test_framework.hpp`).

## Decision
We retain the custom lightweight test framework (`test_framework.hpp`) and integrate it directly with CMake/CTest via single-test execution filters:
1. **Zero External Dependencies:** No package manager (vcpkg, Conan) or git submodule cloning is required to build and execute tests on any platform.
2. **Deterministic Build Speed:** Compilation of the entire test suite takes ~1-2 seconds with GCC or Clang.
3. **Full CTest Granularity:** `test_main.cpp` supports filter arguments (`nettrace_tests "T-1-01"`), allowing CMake to register every individual test ID (`T-1-01` through `T-1-17`) as an isolated CTest target.
4. **Transparent Exception-Based Assertions:** Provides `REQUIRE`, `REQUIRE_EQ`, `REQUIRE_FALSE`, and `CHECK` macros with file/line reporting and clean failure messages.

## Consequences
- Developers do not need to configure external library paths.
- Each test is tracked individually by CTest in both local environments and CI.
- If advanced mocking or parameterization is required in later milestones, GoogleTest or Catch2 may be revisited.
