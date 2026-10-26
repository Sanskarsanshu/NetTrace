#pragma once

#include <cstdint>

namespace nettrace {

/**
 * @brief Ephemeral port allocator per host, per run (M2-52).
 * Allocates ports sequentially in range [49152, 65535], wrapping back to 49152.
 */
class EphemeralPortAllocator {
public:
    static constexpr uint16_t kMinPort = 49152;
    static constexpr uint16_t kMaxPort = 65535;

    explicit EphemeralPortAllocator(uint16_t startPort = kMinPort)
        : nextPort_(startPort) {}

    uint16_t next() {
        uint16_t port = nextPort_;
        if (nextPort_ == kMaxPort) {
            nextPort_ = kMinPort;
        } else {
            ++nextPort_;
        }
        return port;
    }

    uint16_t peek() const {
        return nextPort_;
    }

    void reset(uint16_t port = kMinPort) {
        nextPort_ = port;
    }

private:
    uint16_t nextPort_ = kMinPort;
};

} // namespace nettrace
