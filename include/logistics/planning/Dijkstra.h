#pragma once

#include "logistics/graph/Graph.h"

#include <optional>
#include <unordered_map>
#include <vector>

namespace logistics {

struct PathResult {
    bool reachable{false};
    std::vector<NodeId> node_sequence;
    std::vector<RoadId> road_sequence;
    double total_distance_km{0.0};
    Minutes total_travel_minutes{0.0};
    double total_cost_yuan{0.0};
};

// Immutable snapshot: reconstruction does not depend on the graph's lifetime.
class ShortestPaths {
public:
    std::optional<double> distance_to(const NodeId& target) const;
    PathResult path_to(const NodeId& target) const;

private:
    friend class Dijkstra;
    explicit ShortestPaths(NodeId source);
    NodeId source_;
    std::unordered_map<NodeId, double> distances_;
    std::unordered_map<NodeId, Edge> predecessors_;
};

class Dijkstra {
public:
    static ShortestPaths single_source(const Graph& graph, const NodeId& source,
                                      WeightMetric metric = WeightMetric::Distance);
    static PathResult shortest_path(const Graph& graph, const NodeId& source,
                                    const NodeId& target,
                                    WeightMetric metric = WeightMetric::Distance);
private:
    static ShortestPaths search(const Graph& graph, const NodeId& source, WeightMetric metric,
                                const std::optional<NodeId>& target);
};

}  // namespace logistics
