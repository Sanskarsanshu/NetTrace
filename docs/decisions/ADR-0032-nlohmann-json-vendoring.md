# ADR-0032: Vendoring nlohmann/json 3.11.3 for Topology Serialization

- **Status:** Accepted (locks D-12)
- **Date:** 2026-10-27

## Context
Per Architectural Decision D-12, NetTrace 3D uses C++17, CMake, and `nlohmann/json` for JSON serialization and parsing. The C++ engine must be buildable in both offline environments and continuous integration runners without unpinned network package fetches.

## Decision
1. Vendor `nlohmann/json` version `3.11.3` as a single-header distribution under `engine/third_party/nlohmann/json.hpp` alongside its `LICENSE.MIT`.
2. Include `engine/third_party` via `target_include_directories(nettrace_core SYSTEM PUBLIC ...)` so compiler warnings flags (`-Wall -Wextra -Wpedantic` on GCC/Clang, `/W4` on MSVC) do not trigger spurious third-party warnings.
3. Validate clean compilation and zero sanitizer errors under the Linux GCC AddressSanitizer (ASan) and UndefinedBehaviorSanitizer (UBSan) CI pipeline.

## Consequences
- Guaranteed reproducible, offline builds across developer environments (Windows MSVC) and GitHub Actions (Ubuntu Clang/GCC).
- No external package manager (vcpkg, conan) required for building the engine.
