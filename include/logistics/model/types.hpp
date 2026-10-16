#pragma once

#include <optional>
#include <string>

namespace logistics {

using NodeId = std::string;
using OrderId = std::string;
using RoadId = std::string;
using Minutes = double;  // Minutes since the start of the simulation day.

enum class NodeType { Warehouse, DeliveryPoint, TransferStation };
enum class TrafficStatus { Clear, Congested, Closed };
enum class OrderPriority { Normal, Urgent };
enum class OrderStatus { Pending, Scheduled, Delivered, Late, NeedsAttention };

struct Coordinates {
    double x{0.0};
    double y{0.0};
};

struct Node {
    NodeId id;
    NodeType type{NodeType::DeliveryPoint};
    Coordinates coordinates;
};

struct Road {
    RoadId id;
    NodeId from;
    NodeId to;
    double distance_km{0.0};
    Minutes base_travel_minutes{0.0};
    Minutes current_travel_minutes{0.0};
    double cost_yuan{0.0};
    TrafficStatus traffic_status{TrafficStatus::Clear};
};

struct TimeWindow {
    Minutes earliest{0.0};
    Minutes latest{0.0};
};

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
