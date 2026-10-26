#pragma once

#include "nettrace/model/types.hpp"
#include "nettrace/routing/graph.hpp"
#include "nettrace/routing/route_result.hpp"

namespace nettrace {

class WidestPath {
public:
    struct Options {
        bool excludeInactive = true;
        Options(bool exclude = true) : excludeInactive(exclude) {}
    };

    static RouteResult findPath(const Graph& graph, 
                                const NodeId& source, 
                                const NodeId& destination, 
                                Options options = Options());
};

} // namespace nettrace
