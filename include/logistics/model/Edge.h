#pragma once
#include "logistics/model/common.hpp"

namespace logistics {
class Edge {
public:
    Edge(RoadId id, NodeId from_id, NodeId to_id, double distance, Minutes time,
         double cost, TrafficStatus traffic_status = TrafficStatus::Clear);
    const RoadId& id() const noexcept { return id_; }
    const NodeId& from_id() const noexcept { return from_id_; }
    const NodeId& to_id() const noexcept { return to_id_; }
    double distance() const noexcept { return distance_; }  // km
    Minutes time() const noexcept { return time_; }
    Minutes base_time() const noexcept { return base_time_; }
    double cost() const noexcept { return cost_; }  // yuan
    TrafficStatus traffic_status() const noexcept { return traffic_status_; }
    double weight(WeightMetric metric) const;
    // Validated copy; keeps initial time for future congestion comparisons.
    Edge with_traffic(Minutes time, TrafficStatus status) const;
private:
    RoadId id_;
    NodeId from_id_;
    NodeId to_id_;
    double distance_;
    Minutes time_;
    Minutes base_time_;
    double cost_;
    TrafficStatus traffic_status_;
};
using Road = Edge;  // Stage 1 terminology; only one road representation.
}  // namespace logistics
