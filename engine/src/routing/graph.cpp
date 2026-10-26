#include "nettrace/routing/graph.hpp"
#include <algorithm>

namespace nettrace {

Result<bool> Graph::addNode(NodeId id, std::string name, bool active) {
    if (id.empty()) {
        return Result<bool>::err(ErrorCode::UnknownNode, "Node ID cannot be empty", "node.id");
    }
    if (hasNode(id)) {
        return Result<bool>::err(ErrorCode::DuplicateId, "Node with ID '" + id.str() + "' already exists", "node.id");
    }

    NodeInfo info;
    info.id = id;
    info.name = name.empty() ? id.str() : std::move(name);
    info.active = active;

    nodes_.emplace(id, std::move(info));
    sortedNodeIds_.push_back(id);
    std::sort(sortedNodeIds_.begin(), sortedNodeIds_.end());
    adjacency_[id] = {};

    return Result<bool>::ok(true);
}

bool Graph::hasNode(const NodeId& id) const {
    return nodes_.find(id) != nodes_.end();
}

bool Graph::hasLink(const LinkId& id) const {
    return linkDirectionality_.find(id) != linkDirectionality_.end();
}

const NodeInfo* Graph::getNode(const NodeId& id) const {
    auto it = nodes_.find(id);
    if (it != nodes_.end()) {
        return &it->second;
    }
    return nullptr;
}

Result<bool> Graph::addLink(Edge edge, Directionality direction) {
    if (edge.linkId.empty()) {
        return Result<bool>::err(ErrorCode::DuplicateId, "Link ID cannot be empty", "link.id");
    }
    if (hasLink(edge.linkId)) {
        return Result<bool>::err(ErrorCode::DuplicateId, 
            "Link with ID '" + edge.linkId.str() + "' already exists", "link.id");
    }
    if (edge.from == edge.to) {
        return Result<bool>::err(ErrorCode::SelfLoopRejected, 
            "Self-loop detected on node '" + edge.from.str() + "'; self-loops are rejected", "link.endpoints");
    }
    if (!hasNode(edge.from)) {
        return Result<bool>::err(ErrorCode::DanglingEndpoint, 
            "Link references unknown source node: '" + edge.from.str() + "'", "link.from");
    }
    if (!hasNode(edge.to)) {
        return Result<bool>::err(ErrorCode::DanglingEndpoint, 
            "Link references unknown destination node: '" + edge.to.str() + "'", "link.to");
    }
    if (!isValidCost(edge.cost)) {
        return Result<bool>::err(ErrorCode::InvalidWeight, "Link cost must be finite", "link.cost");
    }
    if (!isValidDelay(edge.delayMs)) {
        return Result<bool>::err(ErrorCode::InvalidWeight, "Link delay must be non-negative and finite", "link.delayMs");
    }
    if (!isValidProbability(edge.lossProbability)) {
        return Result<bool>::err(ErrorCode::InvalidProbability, 
            "Packet loss probability must be in range [0.0, 1.0]", "link.packetLossProbability");
    }
    if (edge.bandwidthBps == 0) {
        return Result<bool>::err(ErrorCode::NonPositiveBandwidth, 
            "Link bandwidth must be strictly positive", "link.bandwidthBps");
    }

    linkDirectionality_[edge.linkId] = direction;

    // Forward arc
    adjacency_[edge.from].push_back(edge);
    std::stable_sort(adjacency_[edge.from].begin(), adjacency_[edge.from].end());

    // Reverse arc for bidirectional links
    if (direction == Directionality::Bidirectional) {
        Edge revEdge = edge;
        revEdge.from = edge.to;
        revEdge.to = edge.from;
        revEdge.fromInterface = edge.toInterface;
        revEdge.toInterface = edge.fromInterface;
        adjacency_[revEdge.from].push_back(revEdge);
        std::stable_sort(adjacency_[revEdge.from].begin(), adjacency_[revEdge.from].end());
    }

    return Result<bool>::ok(true);
}

const std::vector<Edge>& Graph::getEdges(const NodeId& from) const {
    static const std::vector<Edge> empty;
    auto it = adjacency_.find(from);
    if (it != adjacency_.end()) {
        return it->second;
    }
    return empty;
}

std::vector<Edge> Graph::getOutgoingEdges(const NodeId& from, bool excludeInactive) const {
    const NodeInfo* srcNode = getNode(from);
    if (!srcNode || (excludeInactive && !srcNode->active)) {
        return {};
    }

    std::vector<Edge> result;
    auto it = adjacency_.find(from);
    if (it == adjacency_.end()) {
        return result;
    }

    for (const auto& edge : it->second) {
        if (excludeInactive) {
            if (!edge.active) continue;
            const NodeInfo* dstNode = getNode(edge.to);
            if (!dstNode || !dstNode->active) continue;
        }
        result.push_back(edge);
    }
    return result;
}

const Edge* Graph::findEdge(const LinkId& linkId, const NodeId& from, const NodeId& to) const {
    auto it = adjacency_.find(from);
    if (it == adjacency_.end()) return nullptr;
    for (const auto& edge : it->second) {
        if (edge.linkId == linkId && edge.to == to) {
            return &edge;
        }
    }
    return nullptr;
}

Result<bool> Graph::validate() const {
    for (const auto& [nodeId, nodeInfo] : nodes_) {
        if (nodeId.empty()) {
            return Result<bool>::err(ErrorCode::UnknownNode, "Empty node ID found in graph");
        }
    }
    for (const auto& [fromId, edges] : adjacency_) {
        if (!hasNode(fromId)) {
            return Result<bool>::err(ErrorCode::DanglingEndpoint, "Dangling source node in adjacency map: " + fromId.str());
        }
        for (const auto& edge : edges) {
            if (!hasNode(edge.to)) {
                return Result<bool>::err(ErrorCode::DanglingEndpoint, "Dangling target node: " + edge.to.str());
            }
            if (edge.from == edge.to) {
                return Result<bool>::err(ErrorCode::SelfLoopRejected, "Self-loop edge found: " + edge.linkId.str());
            }
        }
    }
    return Result<bool>::ok(true);
}

size_t Graph::totalArcCount() const {
    size_t count = 0;
    for (const auto& [_, edges] : adjacency_) {
        count += edges.size();
    }
    return count;
}

void Graph::clear() {
    nodes_.clear();
    sortedNodeIds_.clear();
    adjacency_.clear();
    linkDirectionality_.clear();
}

} // namespace nettrace
