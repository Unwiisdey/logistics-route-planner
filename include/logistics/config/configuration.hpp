#pragma once

#include "logistics/graph/graph.hpp"
#include "logistics/model/types.hpp"

#include <vector>

namespace logistics {

// Future full simulation configuration. Stage 2 ConfigLoader loads Graph only;
// vehicle and order parsing are deferred to the next stages.
struct Configuration {
    Graph graph;
    std::optional<Vehicle> vehicle;
    std::vector<Order> orders;
    Minutes simulation_start_minutes{0.0};
};

}  // namespace logistics
