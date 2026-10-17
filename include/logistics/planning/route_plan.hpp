#pragma once

#include "logistics/model/types.hpp"

#include <optional>
#include <cstddef>
#include <string>
#include <vector>

namespace logistics {

enum class RouteStrategy { ShortestDistance, LowestCost };
enum class PlanStatus { NotPlanned, Feasible, NoFeasiblePlanFound };
enum class StopRole { Start, Delivery, Return };

struct PlannedStop {
    NodeId node_id;
    std::optional<OrderId> order_id;  // No order for a warehouse stop.
    Minutes estimated_arrival{0.0};
    Minutes estimated_delivery{0.0};
    std::size_t route_node_index{0};
    StopRole role{StopRole::Delivery};
};

// An uncomputed plan must not be reported as a successful empty route.
struct RoutePlan {
    RouteStrategy strategy{RouteStrategy::ShortestDistance};
    PlanStatus status{PlanStatus::NotPlanned};
    NodeId start_node_id;
    NodeId warehouse_id;
    Minutes departure_time{0.0};
    std::vector<NodeId> requested_delivery_node_ids;
    std::vector<NodeId> unassigned_delivery_node_ids;
    std::vector<NodeId> node_sequence;  // Every traversed road endpoint, including transit nodes.
    std::vector<PlannedStop> stops;
    std::vector<RoadId> road_sequence;
    double total_distance_km{0.0};
    double total_cost_yuan{0.0};
    Minutes total_travel_minutes{0.0};
    std::vector<OrderId> unassigned_order_ids;
    std::string diagnostic;
};

}  // namespace logistics
