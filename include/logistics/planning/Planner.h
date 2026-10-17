#pragma once

#include "logistics/graph/Graph.h"
#include "logistics/planning/route_plan.hpp"

#include <optional>
#include <vector>

namespace logistics {

struct PlanningRequest {
    NodeId warehouse_id;
    RouteStrategy strategy{RouteStrategy::ShortestDistance};
    // nullopt means all delivery points; an explicit empty vector means no deliveries.
    std::optional<std::vector<NodeId>> delivery_node_ids;
    // Future replanning can start at a node other than the return warehouse.
    std::optional<NodeId> start_node_id;
    Minutes departure_time{0.0};
    std::optional<double> capacity_kg;
};

class Planner {
public:
    static RoutePlan plan(const Graph& graph, const PlanningRequest& request);
};

}  // namespace logistics
