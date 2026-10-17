#include "logistics/model/Node.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace logistics {
const char* to_string(NodeType type) {
    switch (type) {
    case NodeType::Warehouse: return "warehouse";
    case NodeType::DeliveryPoint: return "delivery_point";
    case NodeType::TransferStation: return "transfer_station";
    }
    throw std::invalid_argument("invalid node type");
}

Node::Node(NodeId id, NodeType type, double x, double y, double demand,
           std::optional<TimeWindow> time_window)
    : id_(std::move(id)), type_(type), x_(x), y_(y), demand_(demand), time_window_(time_window) {
    const auto valid_id_char = [](unsigned char ch) {
        return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
               (ch >= '0' && ch <= '9') || ch == '_' || ch == '-';
    };
    if (id_.empty() || !std::all_of(id_.begin(), id_.end(), valid_id_char)) {
        throw std::invalid_argument("node id must use letters, digits, '_' or '-'");
    }
    (void)to_string(type_);
    if (!std::isfinite(x_) || !std::isfinite(y_)) {
        throw std::invalid_argument("node coordinates must be finite");
    }
    if (!std::isfinite(demand_) || demand_ < 0.0) {
        throw std::invalid_argument("node demand must be finite and nonnegative");
    }
    if (time_window_ &&
        (!std::isfinite(time_window_->earliest) || !std::isfinite(time_window_->latest) ||
         time_window_->earliest < 0.0 || time_window_->earliest > time_window_->latest)) {
        throw std::invalid_argument("time_window requires 0 <= earliest <= latest, both finite");
    }
}
}  // namespace logistics
