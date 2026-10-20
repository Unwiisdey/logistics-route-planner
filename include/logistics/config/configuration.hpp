#pragma once

#include "logistics/graph/graph.hpp"
#include "logistics/model/types.hpp"

#include <vector>

namespace logistics {

// Future unified configuration. Network and order loaders currently remain separate;
// vehicle and clock options are supplied by the application.
struct Configuration {
    Graph graph;
    std::optional<Vehicle> vehicle;
    std::vector<Order> orders;
    Minutes simulation_start_minutes{0.0};
};

}  // namespace logistics
