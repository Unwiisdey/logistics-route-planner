#pragma once
#include <string>

namespace logistics {
using NodeId = std::string;
using OrderId = std::string;
using RoadId = std::string;
using Minutes = double;
enum class NodeType { Warehouse, DeliveryPoint, TransferStation };
enum class TrafficStatus { Clear, Congested, Closed };
enum class OrderPriority { Normal, Urgent };
enum class OrderStatus { Pending, Scheduled, Delivered, Late, NeedsAttention };
enum class WeightMetric { Distance, Time, Cost };
struct TimeWindow {
    Minutes earliest{0.0};
    Minutes latest{0.0};
};
const char* to_string(NodeType type);
const char* to_string(TrafficStatus status);
}  // namespace logistics
