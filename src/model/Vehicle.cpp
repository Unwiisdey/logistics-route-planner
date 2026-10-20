#include "logistics/model/Vehicle.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace logistics {
const char* to_string(VehicleStatus status) {
    switch (status) {
    case VehicleStatus::Idle: return "idle";
    case VehicleStatus::Driving: return "driving";
    case VehicleStatus::Waiting: return "waiting";
    case VehicleStatus::Unloading: return "unloading";
    }
    throw std::invalid_argument("invalid vehicle status");
}
Vehicle::Vehicle(std::string id, double capacity, double load, NodeId warehouse)
    : id_(std::move(id)), capacity_(capacity), load_(load), warehouse_(std::move(warehouse)) {
    if (id_.empty() || warehouse_.empty() || !std::isfinite(capacity) || capacity < 0 ||
        !std::isfinite(load) || load < 0 || load - capacity > 1e-9) {
        throw std::invalid_argument("invalid vehicle ID, warehouse, capacity or load");
    }
    load_ = std::min(load_, capacity_);
    position_.node_id = warehouse_;
}
void Vehicle::depart(const RoadId& road, const NodeId& from, const NodeId& to, Minutes duration) {
    if (!position_.node_id || *position_.node_id != from || !std::isfinite(duration) || duration < 0) {
        throw std::logic_error("vehicle cannot depart from this position");
    }
    segment_ = TravelSegment{road, from, to, duration, duration};
    position_ = {std::nullopt, road, 0};
    status_ = VehicleStatus::Driving;
}
void Vehicle::update_remaining(Minutes remaining) {
    if (!segment_ || !std::isfinite(remaining) || remaining < 0 || remaining > segment_->duration) {
        throw std::logic_error("invalid remaining road time");
    }
    segment_->remaining = remaining;
    position_.edge_progress = segment_->duration == 0 ? 1 : std::clamp(1 - remaining / segment_->duration, 0.0, 1.0);
}
void Vehicle::arrive() {
    if (!segment_) { throw std::logic_error("vehicle has no active segment"); }
    position_ = {segment_->to_id, std::nullopt, 0};
    segment_.reset();
    status_ = VehicleStatus::Idle;
}
void Vehicle::unload(double quantity) {
    if (!std::isfinite(quantity) || quantity < 0 || quantity > load_ + 1e-9 * std::max(1.0, load_)) {
        throw std::logic_error("unloading exceeds current load");
    }
    load_ = std::max(0.0, load_ - quantity);
}
bool Vehicle::can_load(double quantity) const noexcept {
    return std::isfinite(quantity) && quantity >= 0 && quantity - (capacity_ - load_) <= 1e-9;
}
void Vehicle::load(double quantity) {
    if (!position_.node_id || !can_load(quantity)) { throw std::invalid_argument("cannot load cargo: position or capacity"); }
    load_ = std::min(capacity_, load_ + quantity);
}
}  // namespace logistics
