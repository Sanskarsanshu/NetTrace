# NetTrace 3D — C++ Simulation Engine

The authoritative simulation and routing engine for NetTrace 3D, written in ISO C++17.

## Modules

- `model/`: Strong types (`NodeId`, `LinkId`, `SimTimeMs`, `BandwidthBps`), error handling (`Result<T>`), and network entity definitions.
- `routing/`: Graph representations and deterministic routing algorithms (Dijkstra, BFS, Bellman-Ford, Widest-Path).
- `forwarding/`: Layer-3 FIB lookup, longest-prefix-match (LPM), and per-router forwarding decisions.
- `simulation/`: Virtual-clock discrete event simulation, packet state transitions, TTL decrements, and ICMP responses.
- `events/`: Immutable structured event log serialization.

## Building and Testing

### Prerequisites
- GCC 16+ or Clang with C++17 support
- CMake 3.25+ (`python -m cmake` or system `cmake`)
- MinGW Makefiles or Ninja

### Build Instructions
```bash
# Configure
python -m cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build

# Run unit tests
ctest --test-dir build --output-on-failure
```
