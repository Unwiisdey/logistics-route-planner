#include "logistics/model/Edge.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace logistics {
const char* to_string(TrafficStatus status) {
    switch (status) {
    case TrafficStatus::Clear: return "clear";
    case TrafficStatus::Congested: return "congested";
    case TrafficStatus::Closed: return "closed";
    }
    throw std::invalid_argument("invalid traffic status");
}

Edge::Edge(RoadId id, NodeId from_id, NodeId to_id, double distance, Minutes time,
           double cost, TrafficStatus traffic_status)
    : id_(std::move(id)), from_id_(std::move(from_id)), to_id_(std::move(to_id)),
      distance_(distance), time_(time), base_time_(time), cost_(cost), traffic_status_(traffic_status) {
    if (id_.empty() || !std::all_of(id_.begin(), id_.end(), [](unsigned char ch) {
            return ch >= 33 && ch <= 126;
        })) {
        throw std::invalid_argument("edge id must be nonempty printable ASCII without spaces");
    }
    if (from_id_.empty() || to_id_.empty() || from_id_ == to_id_) {
        throw std::invalid_argument("edge endpoints must be nonempty and distinct");
    }
    if (!std::isfinite(distance_) || distance_ < 0.0 || !std::isfinite(time_) ||
        time_ < 0.0 || !std::isfinite(cost_) || cost_ < 0.0) {
        throw std::invalid_argument("edge weights must be finite and nonnegative");
    }
    (void)to_string(traffic_status_);
}

double Edge::weight(WeightMetric metric) const {
    switch (metric) {
    case WeightMetric::Distance: return distance_;
    case WeightMetric::Time: return time_;
    case WeightMetric::Cost: return cost_;
    }
    throw std::invalid_argument("invalid weight metric");
}

Edge Edge::with_traffic(Minutes time, TrafficStatus status) const {
    Edge updated(id_, from_id_, to_id_, distance_, time, cost_, status);
    updated.base_time_ = base_time_;
    return updated;
}
}  // namespace logistics
