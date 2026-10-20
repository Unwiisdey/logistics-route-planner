#pragma once

#include "logistics/model/Node.h"
#include "logistics/model/Edge.h"
#include "logistics/model/Vehicle.h"

#include <optional>
#include <string>

namespace logistics {

struct Order {
    OrderId id;
    NodeId delivery_node_id;
    double quantity{0.0};  // kg
    TimeWindow time_window;
    OrderPriority priority{OrderPriority::Normal};
    OrderStatus status{OrderStatus::Pending};
    std::optional<Minutes> delivered_at;
    double penalty_points{0.0};
};

}  // namespace logistics
