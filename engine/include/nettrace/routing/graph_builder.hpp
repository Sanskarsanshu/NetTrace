#pragma once

#include "nettrace/model/topology.hpp"
#include "nettrace/model/broadcast_domain.hpp"
#include "nettrace/routing/graph.hpp"
#include "nettrace/routing/route_result.hpp"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace nettrace {

/**
 * @brief Metadata for a Layer-3 composite arc formed across a Layer-2 broadcast domain.
 */
struct L3Arc {
    LinkId syntheticEdgeId;
    NodeId fromNode;
    NodeId toNode;
    InterfaceId fromInterface;
    InterfaceId toInterface;
    std::string domainId;
    std::vector<LinkId> underlyingLinkIds;
    std::vector<NodeId> switchesTraversed;

    Cost cost = 0.0;
    DelayMs propagationDelayMs = 0.0;
    BandwidthBps bandwidthBps = 0;
    std::optional<BandwidthBps> availableBandwidthBps;
    Probability packetLossProbability = 0.0;
    uint32_t mtuBytes = 1500;
};

/**
 * @brief Layer-3 collapsed graph containing composite arcs across broadcast domains.
 * Inherits from Graph so M1 algorithms (Dijkstra, BFS, Widest-path, Bellman-Ford) execute unchanged.
 */
class L3Graph : public Graph {
public:
    L3Graph() = default;

    void addL3Arc(L3Arc arc);

    const std::vector<L3Arc>& l3Arcs() const { return l3Arcs_; }
    const L3Arc* findL3Arc(const LinkId& syntheticId) const;
    const L3Arc* findL3Arc(const NodeId& from, const NodeId& to) const;

    // Inactive element exclusions recorded during build
    void addExclusion(std::string exclusion) { exclusions_.push_back(std::move(exclusion)); }
    const std::vector<std::string>& exclusions() const { return exclusions_; }

    struct ExpandedPath {
        std::vector<NodeId> physicalNodeIds;
        std::vector<LinkId> physicalLinkIds;
        std::vector<NodeId> switchesTraversed;
        uint32_t routerHopCount = 0;
        uint32_t linkHopCount = 0;
    };

    // Expands an L3 RouteResult into underlying physical nodes, links, and hop counts (D-05)
    ExpandedPath expandPath(const RouteResult& routeResult, const TopologySnapshot& snap) const;

private:
    std::vector<L3Arc> l3Arcs_;
    std::map<LinkId, L3Arc> arcsById_;
    std::vector<std::string> exclusions_;
};

/**
 * @brief Builds the Layer-3 routing graph from an immutable TopologySnapshot (M2-38).
 */
class GraphBuilder {
public:
    static L3Graph build(const TopologySnapshot& snap);
};

} // namespace nettrace
