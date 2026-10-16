#pragma once

#include "logistics/planning/route_plan.hpp"

#include <cstddef>
#include <vector>

namespace logistics {

struct SimulationClock {
    Minutes now_minutes{0.0};
    bool paused{true};
};

// Snapshot for future simulation, replanning and visualization modules.
struct SimulationState {
    SimulationClock clock;
    Vehicle vehicle;
    std::vector<Order> orders;
    RoutePlan route;
    std::size_t next_stop_index{0};
};

}  // namespace logistics
