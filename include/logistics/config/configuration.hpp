#pragma once

#include "logistics/graph/graph.hpp"

#include <vector>

namespace logistics {

// Data contract for the future configuration loader; parsing is not implemented.
struct Configuration {
    Graph graph;
    Vehicle vehicle;
    std::vector<Order> orders;
    Minutes simulation_start_minutes{0.0};
};

}  // namespace logistics
