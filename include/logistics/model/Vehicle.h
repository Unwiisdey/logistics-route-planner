#pragma once
#include "logistics/model/common.hpp"
#include <optional>

namespace logistics {
enum class VehicleStatus { Idle, Driving, Waiting, Unloading };
const char* to_string(VehicleStatus status);

struct VehiclePosition {
    std::optional<NodeId> node_id;
    std::optional<RoadId> road_id;
    double edge_progress{0.0};
};
struct TravelSegment {
    RoadId road_id;
    NodeId from_id;
    NodeId to_id;
    Minutes duration{0.0};
    Minutes remaining{0.0};
};

// Read-only public state; Simulation alone performs validated transitions.
class Vehicle {
public:
    Vehicle(std::string id, double capacity, double load, NodeId warehouse);
    const std::string& id() const noexcept { return id_; }
    double capacity() const noexcept { return capacity_; }
    double current_load() const noexcept { return load_; }
    const NodeId& home_warehouse_id() const noexcept { return warehouse_; }
    const VehiclePosition& position() const noexcept { return position_; }
    VehicleStatus status() const noexcept { return status_; }
    const std::optional<TravelSegment>& segment() const noexcept { return segment_; }
private:
    friend class Simulation;
    void depart(const RoadId& road, const NodeId& from, const NodeId& to, Minutes duration);
    void update_remaining(Minutes remaining);
    void arrive();
    void unload(double quantity);
    std::string id_;
    double capacity_;
    double load_;
    NodeId warehouse_;
    VehiclePosition position_;
    VehicleStatus status_{VehicleStatus::Idle};
    std::optional<TravelSegment> segment_;
};
}  // namespace logistics
