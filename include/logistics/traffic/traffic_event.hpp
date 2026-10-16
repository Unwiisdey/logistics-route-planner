#pragma once

#include "logistics/model/types.hpp"

namespace logistics {

// Threshold baseline and update policy will be implemented in stage 5.
struct TrafficEvent {
    RoadId road_id;
    Minutes occurred_at{0.0};
    Minutes previous_travel_minutes{0.0};
    Minutes new_travel_minutes{0.0};
    TrafficStatus status{TrafficStatus::Clear};
};

}  // namespace logistics
