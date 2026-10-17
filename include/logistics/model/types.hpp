#pragma once

#include "logistics/model/Node.h"
#include "logistics/model/Edge.h"

#include <optional>
#include <string>

namespace logistics {

struct Order {
    OrderId id;
    NodeId delivery_node_id;
    double quantity{0.0};  // Same unit as vehicle capacity; unit to be decided.
    TimeWindow time_window;
    OrderPriority priority{OrderPriority::Normal};
    OrderStatus status{OrderStatus::Pending};
    std::optional<Minutes> delivered_at;
    double penalty_points{0.0};
};

// Exactly one of node_id / road_id will be set for an initialized position.
// Validation and movement are deferred to later stages.
struct VehiclePosition {
    std::optional<NodeId> node_id;
    std::optional<RoadId> road_id;
    double edge_progress{0.0};  // [0, 1] when moving on a road.
};

struct Vehicle {
    std::string id;
    double capacity{0.0};
    double current_load{0.0};
    NodeId home_warehouse_id;
    VehiclePosition position;
};

}  // namespace logistics
