#pragma once

#include "nettrace/model/types.hpp"
#include "nettrace/model/error.hpp"
#include "nettrace/model/result.hpp"

#include <map>
#include <string>
#include <vector>

namespace nettrace {

struct Edge {
    LinkId linkId;
    NodeId from;
    NodeId to;
    InterfaceId fromInterface;
    InterfaceId toInterface;
    Cost cost = 1.0;
    BandwidthBps bandwidthBps = 1000000000ULL; // 1 Gbps default
    DelayMs delayMs = 1.0;
    Probability lossProbability = 0.0;
    uint32_t mtuBytes = 1500;
    bool active = true;

    // Deterministic ordering: target node first, then cost, then linkId
    bool operator<(const Edge& other) const {
        if (to != other.to) return to < other.to;
        if (cost != other.cost) return cost < other.cost;
        return linkId < other.linkId;
    }
};

struct NodeInfo {
    NodeId id;
    std::string name;
    bool active = true;
};

class Graph {
public:
    Graph() = default;

    // Node management
    Result<bool> addNode(NodeId id, std::string name = "", bool active = true);
    bool hasNode(const NodeId& id) const;
    const NodeInfo* getNode(const NodeId& id) const;
    const std::vector<NodeId>& nodeIds() const { return sortedNodeIds_; }
    size_t nodeCount() const { return nodes_.size(); }

    // Edge / Link management
    Result<bool> addLink(Edge edge, Directionality direction = Directionality::Bidirectional);
    const std::vector<Edge>& getEdges(const NodeId& from) const;
    std::vector<Edge> getOutgoingEdges(const NodeId& from, bool excludeInactive = true) const;
    const Edge* findEdge(const LinkId& linkId, const NodeId& from, const NodeId& to) const;

    // Global inspection & validation
    Result<bool> validate() const;
    size_t totalArcCount() const;
    void clear();

private:
    std::map<NodeId, NodeInfo> nodes_;
    std::vector<NodeId> sortedNodeIds_;
    std::map<NodeId, std::vector<Edge>> adjacency_;
    std::map<LinkId, Directionality> linkDirectionality_;
};

} // namespace nettrace
