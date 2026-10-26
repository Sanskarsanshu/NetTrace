#include "nettrace/forwarding/explain.hpp"
#include <sstream>

namespace nettrace {

Explanation ExplanationGenerator::explain(const ForwardingDecision& decision) {
    Explanation exp;

    std::string destStr = decision.destinationIp.toString();
    std::string nodeStr = decision.nodeId.str();
    std::string outIfaceStr = decision.outInterface.has_value() ? decision.outInterface->str() : "";
    std::string nextHopStr = decision.nextHopIp.has_value() ? decision.nextHopIp->toString() : "";
    std::string matchedPrefixStr = decision.matchedRoute.has_value() ? decision.matchedRoute->prefix.toString() : "";
    std::string routeSourceStr = decision.matchedRoute.has_value() ? std::string(routeSourceToString(decision.matchedRoute->source)) : "";
    bool isOnLink = decision.isDirect();

    // 1. Structured Evidence keys in deterministic order
    exp.evidence.emplace_back("destinationIp", destStr);
    exp.evidence.emplace_back("matchedPrefix", matchedPrefixStr);
    exp.evidence.emplace_back("routeSource", routeSourceStr);
    exp.evidence.emplace_back("nextHopIp", nextHopStr);
    exp.evidence.emplace_back("egressInterface", outIfaceStr);
    exp.evidence.emplace_back("onLink", isOnLink ? "true" : "false");
    exp.evidence.emplace_back("candidateCount", std::to_string(decision.candidateRoutes.size()));

    std::ostringstream rejSs;
    for (size_t i = 0; i < decision.rejectedRoutes.size(); ++i) {
        if (i > 0) rejSs << "; ";
        rejSs << decision.rejectedRoutes[i].first.prefix.toString() << " (" << decision.rejectedRoutes[i].second << ")";
    }
    exp.evidence.emplace_back("rejected", rejSs.str());

    // 2. Structured Explanations
    if (decision.hostOutcome.has_value()) {
        switch (*decision.hostOutcome) {
            case HostSendOutcome::Direct:
                exp.what = "Host sends packet directly to on-link destination " + destStr;
                exp.where = "Node " + nodeStr + " via interface " + outIfaceStr;
                exp.why = "Destination " + destStr + " is on-link in subnet " + matchedPrefixStr;
                exp.whatNext = "Resolve next-hop MAC via ARP or frame encapsulation";
                break;
            case HostSendOutcome::ViaGateway:
                exp.what = "Host forwards packet to default gateway " + nextHopStr;
                exp.where = "Node " + nodeStr + " via interface " + outIfaceStr;
                exp.why = "Destination " + destStr + " is remote; matched " + routeSourceStr + " route " + matchedPrefixStr;
                exp.whatNext = "Transmit packet to gateway " + nextHopStr;
                break;
            case HostSendOutcome::NoRoute:
                exp.what = "Packet dropped: no route to destination";
                exp.where = "Node " + nodeStr;
                if (decision.detail == "ALL_MATCHING_ROUTES_UNUSABLE") {
                    exp.why = "All matching routes are unusable because egress interfaces or links are down";
                } else {
                    exp.why = "No route matching destination " + destStr + " in forwarding table";
                }
                exp.whatNext = "Drop packet and optionally generate ICMP Destination Unreachable";
                break;
        }
    } else if (decision.routerOutcome.has_value()) {
        switch (*decision.routerOutcome) {
            case RouterForwardOutcome::LocalDelivery:
                exp.what = "Packet delivered locally";
                exp.where = "Node " + nodeStr + " interface " + outIfaceStr;
                exp.why = "Destination " + destStr + " matches router's own active interface address";
                exp.whatNext = "Process packet at local transport layer";
                break;
            case RouterForwardOutcome::Forward:
                exp.what = "Router forwards packet to " + nextHopStr;
                exp.where = "Node " + nodeStr + " via interface " + outIfaceStr;
                exp.why = "Matched route " + matchedPrefixStr + " via " + nextHopStr + " with longest prefix match";
                exp.whatNext = "Transmit packet on link to " + nextHopStr;
                break;
            case RouterForwardOutcome::ForwardingDisabled:
                exp.what = "Packet dropped: router forwarding disabled";
                exp.where = "Node " + nodeStr;
                exp.why = "IPv4 forwarding is administratively disabled on router " + nodeStr;
                exp.whatNext = "Drop packet";
                break;
            case RouterForwardOutcome::NoRoute:
                exp.what = "Packet dropped: no route to destination";
                exp.where = "Node " + nodeStr;
                if (decision.detail == "ALL_MATCHING_ROUTES_UNUSABLE") {
                    exp.why = "All matching routes are unusable because egress interfaces or links are down";
                } else {
                    exp.why = "No route matching destination " + destStr + " in forwarding table";
                }
                exp.whatNext = "Drop packet and optionally generate ICMP Destination Unreachable";
                break;
        }
    }

    return exp;
}

} // namespace nettrace
