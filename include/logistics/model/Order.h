#pragma once
#include "logistics/model/common.hpp"
#include <optional>

namespace logistics {
// Immutable request; execution status is recorded separately by Simulation.
class Order {
public:
    Order(OrderId id, NodeId pickup_node, NodeId destination, double quantity,
          std::optional<TimeWindow> window, OrderPriority priority = OrderPriority::Normal,
          Minutes release_time = 0);
    const OrderId& id() const noexcept { return id_; }
    const NodeId& pickup_node_id() const noexcept { return pickup_node_; }
    // Legacy alias for callers using the old warehouse-only request model.
    const NodeId& warehouse_id() const noexcept { return pickup_node_; }
    const NodeId& delivery_node_id() const noexcept { return destination_; }
    double quantity() const noexcept { return quantity_; }
    const std::optional<TimeWindow>& time_window() const noexcept { return window_; }
    OrderPriority priority() const noexcept { return priority_; }
    Minutes release_time() const noexcept { return release_; }
private:
    OrderId id_;
    NodeId pickup_node_;
    NodeId destination_;
    double quantity_;
    std::optional<TimeWindow> window_;
    OrderPriority priority_;
    Minutes release_;
};
}  // namespace logistics
