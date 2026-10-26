#include "nettrace/forwarding/next_hop.hpp"

namespace nettrace {

NextHopResolution resolveNextHop(const ForwardingDecision& decision, const ArpCache& arpCache) {
    NextHopResolution res;

    if (!decision.nextHopIp.has_value() || !decision.outInterface.has_value() || decision.isLocalDelivery()) {
        res.needsArp = false;
        return res;
    }

    res.nextHopIp = *decision.nextHopIp;
    res.egressInterface = *decision.outInterface;

    auto macOpt = arpCache.lookup(res.nextHopIp);
    if (macOpt.has_value()) {
        res.needsArp = false;
        res.cachedMac = *macOpt;
        res.explanation = "ARP cache hit: resolved " + res.nextHopIp.toString() + " to " + macOpt->toString();
    } else {
        res.needsArp = true;
        res.cachedMac = std::nullopt;
        res.explanation = "ARP cache miss for " + res.nextHopIp.toString() + "; broadcast ARP required";
    }

    return res;
}

} // namespace nettrace
