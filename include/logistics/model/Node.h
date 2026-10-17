#pragma once
#include "logistics/model/common.hpp"
#include <optional>

namespace logistics {
// Immutable value object: graph queries cannot bypass validation.
class Node {
public:
    Node(NodeId id, NodeType type, double x, double y, double demand = 0.0,
         std::optional<TimeWindow> time_window = std::nullopt);
    const NodeId& id() const noexcept { return id_; }
    NodeType type() const noexcept { return type_; }
    double x() const noexcept { return x_; }
    double y() const noexcept { return y_; }
    double demand() const noexcept { return demand_; }
    const std::optional<TimeWindow>& time_window() const noexcept { return time_window_; }
private:
    NodeId id_;
    NodeType type_;
    double x_;
    double y_;
    double demand_;
    std::optional<TimeWindow> time_window_;
};
}  // namespace logistics
