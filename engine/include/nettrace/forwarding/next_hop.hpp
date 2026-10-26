#pragma once

#include "nettrace/forwarding/decision.hpp"
#include "nettrace/model/arp_cache.hpp"
#include "nettrace/model/mac.hpp"
#include <optional>
#include <string>

namespace nettrace {

struct NextHopResolution {
    IPv4Address nextHopIp;
    InterfaceId egressInterface;
    bool needsArp = true;
    std::optional<MacAddress> cachedMac;
    std::string explanation;
};

// Evaluates next-hop Layer-2 resolution from a ForwardingDecision and an ArpCache (M2-48).
NextHopResolution resolveNextHop(const ForwardingDecision& decision, const ArpCache& arpCache);

} // namespace nettrace
