#include "logistics/model/Order.h"
#include <cmath>
#include <stdexcept>
#include <utility>

namespace logistics {
Order::Order(OrderId id, NodeId pickup_node, NodeId destination, double quantity,
             std::optional<TimeWindow> window, OrderPriority priority, Minutes release_time)
    : id_(std::move(id)), pickup_node_(std::move(pickup_node)), destination_(std::move(destination)),
      quantity_(quantity), window_(window), priority_(priority), release_(release_time) {
    if (id_.empty() || pickup_node_.empty() || destination_.empty() || !std::isfinite(quantity_) || quantity_ < 0 ||
        !std::isfinite(release_) || release_ < 0 ||
        (priority_ != OrderPriority::Normal && priority_ != OrderPriority::Urgent)) {
        throw std::invalid_argument("invalid order ID, pickup node, destination, quantity, release or priority");
    }
    if (window_) {
        window_->validate();
        if (window_->latest < release_) { throw std::invalid_argument("order deadline precedes release"); }
    }
}
}  // namespace logistics
