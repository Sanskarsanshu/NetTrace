#pragma once

#include "nettrace/model/types.hpp"
#include "nettrace/model/error.hpp"
#include <string>
#include <vector>

namespace nettrace {

struct RouteResult {
    bool reachable = false;
    std::vector<NodeId> pathNodeIds;
    std::vector<LinkId> pathLinkIds;
    
    // Explicit objective name, value, and unit to avoid numeric overloading
    std::string objectiveName;
    double objectiveValue = 0.0;
    std::string objectiveUnit;

    Cost totalCost = 0.0;
    uint32_t linkHopCount = 0;
    uint32_t routerHopCount = 0;
    
    // Algorithm telemetry
    uint32_t exploredNodeCount = 0;
    uint32_t relaxationCount = 0;
    double algorithmRuntimeMs = 0.0;
    
    // Bottleneck and loss estimates for reconstructed path
    BandwidthBps bottleneckBandwidthBps = 0;
    DelayMs estimatedDelayMs = 0.0;
    Probability estimatedLossProbability = 0.0;

    // Failures and cycles
    ErrorCode failureCode = ErrorCode::None;
    std::string failureReason;
    std::vector<NodeId> cycleNodeIds; // For negative cycle reporting

    std::vector<std::string> warnings;

    static RouteResult unreachable(std::string objective, std::string unit, ErrorCode code, std::string reason) {
        RouteResult r;
        r.reachable = false;
        r.objectiveName = std::move(objective);
        r.objectiveUnit = std::move(unit);
        r.failureCode = code;
        r.failureReason = std::move(reason);
        return r;
    }
};

} // namespace nettrace
