#pragma once

#include "logistics/model/types.hpp"

#include <optional>
#include <string>
#include <vector>

namespace logistics {

enum class RouteStrategy { ShortestDistance, LowestCost };
enum class PlanStatus { NotPlanned, Feasible, NoFeasiblePlanFound };

struct PlannedStop {
    NodeId node_id;
    std::optional<OrderId> order_id;  // No order for a warehouse stop.
    Minutes estimated_arrival{0.0};
    Minutes estimated_delivery{0.0};
};

// An uncomputed plan must not be reported as a successful empty route.
struct RoutePlan {
    RouteStrategy strategy{RouteStrategy::ShortestDistance};
    PlanStatus status{PlanStatus::NotPlanned};
    std::vector<PlannedStop> stops;
    std::vector<RoadId> road_sequence;
    double total_distance_km{0.0};
    double total_cost_yuan{0.0};
    Minutes total_travel_minutes{0.0};
    std::vector<OrderId> unassigned_order_ids;
    std::string diagnostic;
};

}  // namespace logistics
