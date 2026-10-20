#include "logistics/model/Order.h"
#include <cmath>
#include <stdexcept>
#include <utility>

namespace logistics {
Order::Order(OrderId id, NodeId warehouse, NodeId destination, double quantity,
             std::optional<TimeWindow> window, OrderPriority priority, Minutes release_time)
    : id_(std::move(id)), warehouse_(std::move(warehouse)), destination_(std::move(destination)),
      quantity_(quantity), window_(window), priority_(priority), release_(release_time) {
    if (id_.empty() || warehouse_.empty() || destination_.empty() || !std::isfinite(quantity_) || quantity_ < 0 ||
        !std::isfinite(release_) || release_ < 0 ||
        (priority_ != OrderPriority::Normal && priority_ != OrderPriority::Urgent)) {
        throw std::invalid_argument("invalid order ID, warehouse, destination, quantity, release or priority");
    }
    if (window_) {
        window_->validate();
        if (window_->latest < release_) { throw std::invalid_argument("order deadline precedes release"); }
    }
}
}  // namespace logistics
